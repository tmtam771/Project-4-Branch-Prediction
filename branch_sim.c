/* Project 4 main harness. Predict ALL before update; shift shared GHR once. */
#include "branch_sim.h"
#include "tage.h"
#include "tournament.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

static Branch trace_data[MAX_TRACE];
static Predictor models[MAX_PREDICTORS];

enum { SHARED_HISTORY_BITS = 32 };

static bool parse_instruction_count(const char *arg, uint64_t *number) {
    char *end;
    errno=0;
    unsigned long long tmp=strtoull(arg,&end,10);
    if(errno || !arg[0] || *end || tmp==0) return false;
    *number=(uint64_t)tmp;
    return true;
}

static void print_result(const char *name, uint64_t correct, uint64_t mispred,
                         int n, uint64_t instructions, uint64_t bits) {
    double acc=100.0*correct/(double)n;
    double mpkb=1000.0*mispred/(double)n;
    printf("%-29s %4" PRIu64 "/%-3d %7.2f%% %9.2f ",name,correct,n,acc,mpkb);
    if(instructions) printf("%10.2f ",1000.0*mispred/(double)instructions);
    else printf("%10s ","N/A");
    printf("%8" PRIu64 "\n",bits);
}

int main(int argc, char **argv) {
    char *trace_file=NULL;
    uint64_t instructions=0;
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--instructions")==0) {
            if(++i>=argc || !parse_instruction_count(argv[i],&instructions)) {
                fprintf(stderr,"Usage: %s [trace.txt] [--instructions VERIFIED_COUNT]\n",argv[0]);
                return 1;
            }
        } else if(argv[i][0]=='-' || trace_file) {
            fprintf(stderr,"Usage: %s [trace.txt] [--instructions VERIFIED_COUNT]\n",argv[0]);
            return 1;
        } else trace_file=argv[i];
    }
    int n=trace_file ? load_trace(trace_file,trace_data,MAX_TRACE)
                     : built_in_trace(trace_data,MAX_TRACE);
    if(n<=0) { fprintf(stderr,"No valid branches to simulate\n"); return 1; }
    if(instructions && instructions<(uint64_t)n) {
        fprintf(stderr,"Instruction count cannot be less than branch count (%d)\n",n);
        return 1;
    }
    uint64_t taken=0;
    for(int i=0;i<n;i++) if(trace_data[i].taken) taken++;
    printf("Trace: %s | branches=%d | taken=%" PRIu64 " | not-taken=%" PRIu64 "\n",
           trace_file ? trace_file : "built-in", n, taken, (uint64_t)n-taken);
    if(!instructions) printf("Note: instruction count unknown; MPKI=N/A. MPKB shown instead.\n");

    int m=0;
    init_model(&models[m++],ALWAYS_T,"Always-Taken",0,0,0,0);
    init_model(&models[m++],ALWAYS_N,"Always-Not-Taken",0,0,0,0);
    init_model(&models[m++],ONE_BIT,"1-bit (size=64)",64,0,0,0);
    init_model(&models[m++],ONE_BIT,"1-bit (size=256)",256,0,0,0);
    init_model(&models[m++],TWO_BIT,"2-bit (size=64)",64,0,0,0);
    init_model(&models[m++],TWO_BIT,"2-bit (size=256)",256,0,0,0);
    init_model(&models[m++],GSHARE,"GShare (h=4,size=64)",64,4,0,0);
    init_model(&models[m++],GSHARE,"GShare (h=8,size=256)",256,8,0,0);
#ifndef BRANCH_SIM_USE_PERCEPTRON
    const int gshare_component=m;
#endif
    init_model(&models[m++],GSHARE,"GShare (h=12,size=1024)",1024,12,0,0);
    init_model(&models[m++],TOURNEY_LAB4,"Lab4 Tournament (1-bit)",256,12,1024,256);

    Tournament tournament;
    tournament_init(&tournament);
    tage_init();
    uint64_t tage_correct=0, tournament_correct=0;
#ifdef BRANCH_SIM_USE_PERCEPTRON
    perceptron_init();
    uint64_t perceptron_correct=0;
    const char *tournament_name="Tournament (TAGE+Perceptron)";
#else
    const char *tournament_name="Tournament (TAGE+GShare)";
#endif

    /* One shared GHR. Predict all models before any update; shift ONCE per branch. */
    uint64_t ghr=0;
    for(int i=0;i<n;i++) {
        const BranchInput input={trace_data[i].pc,ghr};
        const bool actual=trace_data[i].taken;
        bool guesses[MAX_PREDICTORS];
        for(int j=0;j<m;j++) guesses[j]=predict(&models[j],input.pc,input.history);
        const bool prediction_a=tage_predict(input);
#ifdef BRANCH_SIM_USE_PERCEPTRON
        const bool prediction_b=perceptron_predict(input);
        if(prediction_b==actual) perceptron_correct++;
#else
        const bool prediction_b=guesses[gshare_component];
#endif
        const bool tournament_guess=tournament_predict(&tournament,input.pc,
                                                        prediction_a,prediction_b);

        for(int j=0;j<m;j++) {
            if(guesses[j]==actual) models[j].correct++;
            else models[j].mispred++;
        }
        if(prediction_a==actual) tage_correct++;
        if(tournament_guess==actual) tournament_correct++;

        tournament_update(&tournament,input.pc,prediction_a,prediction_b,actual);
        for(int j=0;j<m;j++) update(&models[j],input.pc,input.history,actual);
        tage_update(input,actual);
#ifdef BRANCH_SIM_USE_PERCEPTRON
        perceptron_update(input,actual);
#endif
        ghr=((ghr<<1)|(uint64_t)actual) & ((UINT64_C(1)<<SHARED_HISTORY_BITS)-1);
    }

    printf("\n%-29s %8s %8s %9s %10s %8s\n",
           "Predictor","Correct","Acc%","MPKB","MPKI","Bits");
    printf("--------------------------------------------------------------------------\n");
    for(int j=0;j<m;j++) {
        const Predictor *p=&models[j];
        print_result(p->name,p->correct,p->mispred,n,instructions,storage_bits(p));
    }
    const uint64_t tage_bits=tage_storage_bits()+SHARED_HISTORY_BITS;
    print_result("TAGE",tage_correct,(uint64_t)n-tage_correct,n,instructions,tage_bits);
#ifdef BRANCH_SIM_USE_PERCEPTRON
    const uint64_t component_bits=perceptron_storage_bits();
    print_result("Perceptron",perceptron_correct,(uint64_t)n-perceptron_correct,
                 n,instructions,component_bits+SHARED_HISTORY_BITS);
#else
    const Predictor *gshare=&models[gshare_component];
    const uint64_t component_bits=storage_bits(gshare)-gshare->history_bits;
#endif
    print_result(tournament_name,tournament_correct,(uint64_t)n-tournament_correct,
                 n,instructions,tage_bits+component_bits+tournament_chooser_bits());
    printf("\n[DONE]\n");
    return 0;
}
