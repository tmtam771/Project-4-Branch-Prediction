
/*
 * Project 4 - Phase 1: Unified baseline simulator
 * Ported from Lab 4. The original code is retained in lab4_branch_sim_backup.c.
 *
 * Trace format: 0x400120 T (or N); one branch per line.
 * IMPORTANT: Branch-only traces do NOT reveal total instruction count.
 * Therefore the default metric is MPKB (mispredictions per 1000 branches).
 * Real MPKI is displayed only with --instructions <verified_instruction_count>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#define MAX_TRACE 500000
#define MAX_TABLE 4096
#define MAX_PREDICTORS 10

typedef struct { uint64_t pc; bool taken; } Branch;
typedef enum { ALWAYS_T, ALWAYS_N, ONE_BIT, TWO_BIT, GSHARE, TOURNEY_LAB4 } Kind;

typedef struct {
    Kind kind;
    char name[64];
    int size;                  /* Classic or 2-bit table size */
    int gshare_size;           /* For Lab 4 tournament */
    int history_bits;
    int selector_size;
    uint8_t table[MAX_TABLE];
    uint8_t gshare_table[MAX_TABLE];
    uint8_t selector[MAX_TABLE];
    uint64_t correct, mispred;
} Predictor;

static Branch trace_data[MAX_TRACE];
static Predictor models[MAX_PREDICTORS];

static void init_model(Predictor *p, Kind kind, const char *name,
                       int size, int hbits, int gsize, int selsize) {
    memset(p, 0, sizeof(*p));
    p->kind = kind;
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->size = size;
    p->history_bits = hbits;
    p->gshare_size = gsize;
    p->selector_size = selsize;
    memset(p->table, (kind == ONE_BIT) ? 1 : 2, sizeof(p->table));
    memset(p->gshare_table, 2, sizeof(p->gshare_table));
    memset(p->selector, 1, sizeof(p->selector)); /* favor GShare initially */
}

static uint32_t idx_pc(uint64_t pc, int size) {
    return (uint32_t)(pc % (uint64_t)size);
}
static uint32_t idx_gshare(uint64_t pc, uint64_t history, int bits, int size) {
    uint64_t mask = (UINT64_C(1) << bits) - 1;
    return (uint32_t)((pc ^ (history & mask)) % (uint64_t)size);
}
static bool predict(const Predictor *p, uint64_t pc, uint64_t history) {
    uint32_t b, g, s;
    switch(p->kind) {
    case ALWAYS_T: return true;
    case ALWAYS_N: return false;
    case ONE_BIT: return p->table[idx_pc(pc,p->size)] != 0;
    case TWO_BIT: return p->table[idx_pc(pc,p->size)] >= 2;
    case GSHARE: return p->table[idx_gshare(pc,history,p->history_bits,p->size)] >= 2;
    case TOURNEY_LAB4:
        b = idx_pc(pc,p->size);
        g = idx_gshare(pc,history,p->history_bits,p->gshare_size);
        s = idx_pc(pc >> 2,p->selector_size);
        return p->selector[s] ? (p->gshare_table[g] >= 2) : (p->table[b] >= 2);
    }
    return false;
}
static void train_2bit(uint8_t *ctr, bool actual) {
    if (actual && *ctr < 3) ++*ctr;
    else if (!actual && *ctr > 0) --*ctr;
}
static void update(Predictor *p, uint64_t pc, uint64_t history, bool actual) {
    uint32_t b, g, s;
    bool bp, gp;
    switch(p->kind) {
    case ALWAYS_T: case ALWAYS_N: return;
    case ONE_BIT:
        p->table[idx_pc(pc,p->size)] = (uint8_t)actual; return;
    case TWO_BIT:
        train_2bit(&p->table[idx_pc(pc,p->size)], actual); return;
    case GSHARE:
        train_2bit(&p->table[idx_gshare(pc,history,p->history_bits,p->size)], actual); return;
    case TOURNEY_LAB4:
        b = idx_pc(pc,p->size);
        g = idx_gshare(pc,history,p->history_bits,p->gshare_size);
        s = idx_pc(pc >> 2,p->selector_size);
        bp = p->table[b] >= 2;
        gp = p->gshare_table[g] >= 2;
        if (bp != gp) p->selector[s] = (uint8_t)(gp == actual);
        train_2bit(&p->table[b],actual);
        train_2bit(&p->gshare_table[g],actual);
        return;
    }
}
static uint64_t storage_bits(const Predictor *p) {
    switch(p->kind) {
    case ALWAYS_T: case ALWAYS_N: return 0;
    case ONE_BIT: return (uint64_t)p->size;
    case TWO_BIT: return 2ULL * p->size;
    case GSHARE: return 2ULL * p->size + p->history_bits;
    case TOURNEY_LAB4:
        return 2ULL*p->size + 2ULL*p->gshare_size + p->history_bits + p->selector_size;
    }
    return 0;
}

/* Keep the original Lab 4 built-in trace behavior for regression testing. */
static int built_in_trace(Branch *out, int max) {
    int n=0;
    for(int rep=0;rep<20 && n<max;rep++) {
        for(int k=0;k<9 && n<max;k++) out[n++] = (Branch){0x100,true};
        if(n<max) out[n++] = (Branch){0x100,false};
    }
    for(int k=0;k<80 && n<max;k++) out[n++] = (Branch){0x200,k%2==0};
    for(int k=0;k<100 && n<max;k++) out[n++] = (Branch){0x300,k%3!=2};
    unsigned seed=42;
    for(int k=0;k<200 && n<max;k++) {
        seed=seed*1664525u+1013904223u;
        uint64_t pc=(uint64_t)((seed&0xfffu)<<2);
        seed=seed*1664525u+1013904223u;
        out[n++] = (Branch){pc,((seed>>31)&1)!=0};
    }
    return n;
}

static int load_trace(const char *path, Branch *out, int max) {
    FILE *f=fopen(path,"r");
    if(!f) { perror(path); return -1; }
    int n=0, lineno=0;
    char line[256];
    while(fgets(line,sizeof(line),f)) {
        lineno++;
        char *p=line;
        while(isspace((unsigned char)*p)) p++;
        if(*p=='#' || *p=='\0') continue;
        if(n>=max) { fprintf(stderr,"Trace exceeds MAX_TRACE (%d)\n",max); fclose(f); return -1; }
        char token[64], outcome[16], extra[2];
        unsigned long long pc=0;
        int fields=sscanf(p,"%63s %15s %1s",token,outcome,extra);
        if(fields==1) {
            if (strlen(token) >= sizeof(outcome)) {
                fprintf(stderr,"Invalid outcome at line %d\n",lineno);
                fclose(f); return -1;
            }
            strcpy(outcome,token);
            pc=(unsigned long long)n; /* compatible with original Lab 4 single T/N line */
        } else if(fields==2) {
            errno=0;
            char *end;
            pc=strtoull(token,&end,0);
            if(errno || *end) {
                fprintf(stderr,"Invalid PC at line %d\n",lineno); fclose(f); return -1;
            }
        } else {
            fprintf(stderr,"Invalid trace format at line %d\n",lineno); fclose(f); return -1;
        }
        if(strcmp(outcome,"T") && strcmp(outcome,"t") && strcmp(outcome,"1") &&
           strcmp(outcome,"N") && strcmp(outcome,"n") && strcmp(outcome,"0")) {
            fprintf(stderr,"Invalid outcome at line %d (use T or N)\n",lineno);
            fclose(f); return -1;
        }
        out[n++] = (Branch){(uint64_t)pc, outcome[0]=='T'||outcome[0]=='t'||outcome[0]=='1'};
    }
    fclose(f);
    return n;
}
static bool parse_instruction_count(const char *arg, uint64_t *number) {
    char *end;
    errno=0;
    unsigned long long tmp=strtoull(arg,&end,10);
    if(errno || !arg[0] || *end || tmp==0) return false;
    *number=(uint64_t)tmp;
    return true;
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
    init_model(&models[m++],GSHARE,"GShare (h=12,size=1024)",1024,12,0,0);
    init_model(&models[m++],TOURNEY_LAB4,"Lab4 Tournament (1-bit)",256,12,1024,256);

    /* One shared GHR. Predict all models before any update; shift ONCE per branch. */
    uint64_t ghr=0;
    for(int i=0;i<n;i++) {
        bool guesses[MAX_PREDICTORS];
        for(int j=0;j<m;j++) guesses[j]=predict(&models[j],trace_data[i].pc,ghr);
        for(int j=0;j<m;j++) {
            if(guesses[j]==trace_data[i].taken) models[j].correct++;
            else models[j].mispred++;
        }
        for(int j=0;j<m;j++) update(&models[j],trace_data[i].pc,ghr,trace_data[i].taken);
        ghr=((ghr<<1)|(trace_data[i].taken?1ULL:0ULL)) & UINT64_C(0xffff);
    }

    printf("\n%-29s %8s %8s %9s %10s %8s\n",
           "Predictor","Correct","Acc%","MPKB","MPKI","Bits");
    printf("--------------------------------------------------------------------------\n");
    for(int j=0;j<m;j++) {
        const Predictor *p=&models[j];
        double acc=100.0*p->correct/(double)n;
        double mpkb=1000.0*p->mispred/(double)n;
        printf("%-29s %4" PRIu64 "/%-3d %7.2f%% %9.2f ",p->name,p->correct,n,acc,mpkb);
        if(instructions) printf("%10.2f ",1000.0*p->mispred/(double)instructions);
        else printf("%10s ","N/A");
        printf("%8" PRIu64 "\n",storage_bits(p));
    }
    printf("\n[DONE]\n");
    return 0;
}
