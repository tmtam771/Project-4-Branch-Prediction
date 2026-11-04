/*
 * ============================================================
 *  Lab 4 – Branch Predictor Simulator  (C)
 *  Computer Architecture Course
 * ============================================================
 *
 *  Implements and compares four branch predictors:
 *    1. Always-Taken / Always-Not-Taken  (static baselines)
 *    2. 1-bit predictor  (per-branch last-outcome table)
 *    3. 2-bit saturating counter  (4-state FSM per entry)
 *    4. GShare  (global history XOR PC indexes 2-bit table)
 *    5. Tournament (1-bit selector chooses 2-bit or GShare)
 *
 *  Input: branch trace file — one outcome per line:
 *    T  (taken)  or  N  (not taken)
 *    Optional hex PC prefix:  0x400100 T
 *
 *  Build:
 *    gcc -Wall -O2 -o branch_sim branch_sim.c
 *
 *  Run:
 *    ./branch_sim               # built-in synthetic trace
 *    ./branch_sim trace.txt     # load trace file
 *    ./branch_sim --sweep       # sensitivity sweep (table size / history)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── Limits ─────────────────────────────────────────────── */
#define MAX_TRACE   500000
#define MAX_TABLE   4096     /* max predictor table entries  */

/* ─── Trace entry ────────────────────────────────────────── */
typedef struct { uint32_t pc; bool taken; } BrEntry;

/* ─── Predictor statistics ───────────────────────────────── */
typedef struct {
    uint64_t total, correct, mispred;
    char     name[48];
} PredStats;

static void stats_init(PredStats *s, const char *name){
    memset(s,0,sizeof(*s)); strncpy(s->name,name,47);
}
static void stats_record(PredStats *s, bool pred, bool actual){
    s->total++;
    if(pred==actual) s->correct++;
    else             s->mispred++;
}
static void stats_print(PredStats *s){
    printf("  %-30s  correct=%6llu/%6llu  acc=%5.2f%%  MPKI=%6.2f\n",
           s->name,
           (unsigned long long)s->correct,
           (unsigned long long)s->total,
           s->total?100.0*s->correct/s->total:0.0,
           s->total?1000.0*s->mispred/s->total:0.0);
}

/* ─── 1. Always-Taken / Always-Not-Taken ─────────────────── */
static void predict_static(BrEntry *tr, int n){
    PredStats sa, sn;
    stats_init(&sa,"Always-Taken");
    stats_init(&sn,"Always-Not-Taken");
    for(int i=0;i<n;i++){
        stats_record(&sa,true,  tr[i].taken);
        stats_record(&sn,false, tr[i].taken);
    }
    stats_print(&sa); stats_print(&sn);
}

/* ─── 2. 1-bit predictor ─────────────────────────────────── */
static void predict_1bit(BrEntry *tr, int n, int tsize){
    static uint8_t tbl[MAX_TABLE];
    memset(tbl,1,sizeof(tbl));   /* init: predict taken */
    PredStats s; char name[48];
    snprintf(name,48,"1-bit (size=%d)",tsize);
    stats_init(&s,name);
    for(int i=0;i<n;i++){
        int idx=(int)(tr[i].pc%(uint32_t)tsize);
        bool pred=(tbl[idx]!=0);
        stats_record(&s,pred,tr[i].taken);
        tbl[idx]=(uint8_t)tr[i].taken;
    }
    stats_print(&s);
}

/* ─── 3. 2-bit saturating counter ───────────────────────────
 *  States: 0=SN  1=WN  2=WT  3=ST
 *  Predict taken if state >= 2
 */
static void predict_2bit(BrEntry *tr, int n, int tsize){
    static uint8_t tbl[MAX_TABLE];
    memset(tbl,2,sizeof(tbl));   /* init: weakly taken */
    PredStats s; char name[48];
    snprintf(name,48,"2-bit saturating (size=%d)",tsize);
    stats_init(&s,name);
    for(int i=0;i<n;i++){
        int idx=(int)(tr[i].pc%(uint32_t)tsize);
        bool pred=(tbl[idx]>=2);
        stats_record(&s,pred,tr[i].taken);
        if(tr[i].taken){ if(tbl[idx]<3) tbl[idx]++; }
        else            { if(tbl[idx]>0) tbl[idx]--; }
    }
    stats_print(&s);
}

/* ─── 4. GShare ──────────────────────────────────────────────
 *  global history register XOR'd with PC indexes 2-bit table
 */
static void predict_gshare(BrEntry *tr, int n, int hist_bits, int tsize){
    static uint8_t tbl[MAX_TABLE];
    memset(tbl,2,sizeof(tbl));
    uint32_t hist=0;
    uint32_t hist_mask=(1u<<hist_bits)-1;
    PredStats s; char name[48];
    snprintf(name,48,"GShare (h=%d,size=%d)",hist_bits,tsize);
    stats_init(&s,name);
    for(int i=0;i<n;i++){
        int idx=(int)((tr[i].pc^hist)%(uint32_t)tsize);
        bool pred=(tbl[idx]>=2);
        stats_record(&s,pred,tr[i].taken);
        if(tr[i].taken){ if(tbl[idx]<3) tbl[idx]++; }
        else            { if(tbl[idx]>0) tbl[idx]--; }
        hist=((hist<<1)|(tr[i].taken?1u:0u))&hist_mask;
    }
    stats_print(&s);
}

/* --- Task 4: Tournament predictor (1-bit selector per PC index) ---
 *  selector = 0 -> use 2-bit saturating predictor
 *  selector = 1 -> use GShare
 *  Both component predictors are trained after every branch.
 *  Selector is trained only when the two predictions disagree.
 *  The PC-indexed selector is a finite table, so different PCs can alias.
 */
static void predict_tournament(BrEntry *tr, int n,
                               int bim_size, int hist_bits, int gs_size,
                               int selector_size){
    static uint8_t bim_table[MAX_TABLE];
    static uint8_t gs_table[MAX_TABLE];
    static uint8_t selector[MAX_TABLE];

    memset(bim_table, 2, sizeof(bim_table)); /* weakly taken */
    memset(gs_table, 2, sizeof(gs_table));   /* weakly taken */
    memset(selector, 1, sizeof(selector));   /* initially choose GShare */

    uint32_t history = 0;
    uint32_t history_mask = (1u << hist_bits) - 1u;
    PredStats result;
    stats_init(&result, "Tournament (1-bit selector)");

    for(int i = 0; i < n; ++i){
        uint32_t pc = tr[i].pc;
        bool actual = tr[i].taken;
        int bidx = (int)(pc % (uint32_t)bim_size);
        int gidx = (int)((pc ^ history) % (uint32_t)gs_size);
        int sidx = (int)((pc >> 2) % (uint32_t)selector_size);

        /* Step 1: compute BOTH predictions using the old history. */
        bool bpred = bim_table[bidx] >= 2;
        bool gpred = gs_table[gidx] >= 2;

        /* Step 2: 1-bit PC-indexed selector chooses the final answer. */
        bool final_pred = selector[sidx] ? gpred : bpred;
        stats_record(&result, final_pred, actual);

        /* Step 3: update selector only if the predictors disagree. */
        if(bpred != gpred){
            selector[sidx] = (uint8_t)(gpred == actual);
        }

        /* Step 4: train BOTH component predictors, selected or not. */
        if(actual){
            if(bim_table[bidx] < 3) ++bim_table[bidx];
            if(gs_table[gidx] < 3) ++gs_table[gidx];
        }else{
            if(bim_table[bidx] > 0) --bim_table[bidx];
            if(gs_table[gidx] > 0) --gs_table[gidx];
        }

        /* Step 5: update global history LAST (no future leakage). */
        history = ((history << 1) | (actual ? 1u : 0u)) & history_mask;
    }

    stats_print(&result);
}

/* ─── Trace generation (built-in) ───────────────────────────
 *  Mixes:  tight loop (taken 9/10), alternating, random
 */
static int gen_trace(BrEntry *buf, int max){
    int n=0;
    /* loop branches: PC=0x100, taken 9 times then exit */
    for(int rep=0;rep<20&&n<max;rep++){
        for(int k=0;k<9&&n<max;k++){ buf[n].pc=0x100;buf[n].taken=true; n++; }
        if(n<max){ buf[n].pc=0x100;buf[n].taken=false;n++; } /* exit */
    }
    /* alternating: PC=0x200 */
    for(int k=0;k<80&&n<max;k++){ buf[n].pc=0x200;buf[n].taken=(k%2==0);n++; }
    /* if-else: PC=0x300, taken 60% */
    for(int k=0;k<100&&n<max;k++){
        /* simple pattern: T T N T T N ... */
        buf[n].pc=0x300; buf[n].taken=(k%3!=2); n++;
    }
    /* random: PC varies */
    unsigned seed=42;
    for(int k=0;k<200&&n<max;k++){
        seed=seed*1664525+1013904223;
        buf[n].pc=(seed&0xFFF)<<2;
        seed=seed*1664525+1013904223;
        buf[n].taken=(seed>>31)&1; n++;
    }
    return n;
}

/* ─── Load trace file ────────────────────────────────────── */
static int load_trace(const char *path, BrEntry *buf, int max){
    FILE*f=fopen(path,"r");if(!f){perror(path);return-1;}
    int n=0; char ln[64];
    while(fgets(ln,sizeof(ln),f)&&n<max){
        /* skip comments */
        if(ln[0]=='#'||ln[0]=='\n') continue;
        unsigned long pc=0; char outcome[4]="N";
        bool has_pc=(sscanf(ln,"%lx %3s",&pc,outcome)==2);
        if(!has_pc){ sscanf(ln," %3s",outcome); pc=(unsigned long)n; }
        buf[n].pc=(uint32_t)pc;
        buf[n].taken=(outcome[0]=='T'||outcome[0]=='t'||outcome[0]=='1');
        n++;
    }
    fclose(f);return n;
}

/* ─── Sensitivity sweep ──────────────────────────────────── */
static void sweep(BrEntry *tr, int n){
    printf("\n── GShare Sensitivity: history bits ────────────────\n");
    printf("  %-30s %7s %7s\n","Config","Acc%","MPKI");
    int sizes[]={4,8,16,32,64,128,256,1024};
    for(int h=1;h<=12;h++){
        int tsize=1<<(h+3); if(tsize>MAX_TABLE) tsize=MAX_TABLE;
        static uint8_t tbl[MAX_TABLE]; memset(tbl,2,sizeof(tbl));
        uint32_t hist=0,mask=(1u<<h)-1;
        uint64_t tot=0,ok=0;
        for(int i=0;i<n;i++){
            int idx=(int)((tr[i].pc^hist)%(uint32_t)tsize);
            bool pred=(tbl[idx]>=2); tot++;
            if(pred==tr[i].taken) ok++;
            if(tr[i].taken){if(tbl[idx]<3)tbl[idx]++;}
            else{if(tbl[idx]>0)tbl[idx]--;}
            hist=((hist<<1)|(tr[i].taken?1u:0u))&mask;
        }
        char name[48]; snprintf(name,48,"GShare h=%d tbl=%d",h,tsize);
        printf("  %-30s %6.2f%% %7.2f\n",name,
               tot?100.0*ok/tot:0.0, tot?1000.0*(tot-ok)/tot:0.0);
    }
    printf("\n── 2-bit Table Size Sensitivity ───────────────────\n");
    printf("  %-30s %7s %7s\n","Config","Acc%","MPKI");
    for(int i=0;i<(int)(sizeof(sizes)/sizeof(sizes[0]));i++){
        int tsize=sizes[i];
        static uint8_t tbl2[MAX_TABLE]; memset(tbl2,2,sizeof(tbl2));
        uint64_t tot=0,ok=0;
        for(int j=0;j<n;j++){
            int idx=(int)(tr[j].pc%(uint32_t)tsize);
            bool pred=(tbl2[idx]>=2); tot++;
            if(pred==tr[j].taken) ok++;
            if(tr[j].taken){if(tbl2[idx]<3)tbl2[idx]++;}
            else{if(tbl2[idx]>0)tbl2[idx]--;}
        }
        char name[48]; snprintf(name,48,"2-bit size=%d",tsize);
        printf("  %-30s %6.2f%% %7.2f\n",name,
               tot?100.0*ok/tot:0.0, tot?1000.0*(tot-ok)/tot:0.0);
    }
}

/* ─── Main ──────────────────────────────────────────────── */
int main(int argc, char *argv[]){
    static BrEntry trace[MAX_TRACE]; int n;
    bool do_sweep=false; char *tfile=NULL;

    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--sweep")) do_sweep=true;
        else tfile=argv[i];
    }

    if(tfile){ n=load_trace(tfile,trace,MAX_TRACE);if(n<0)return 1;
        printf("Loaded %d branches from %s\n",n,tfile);
    } else {
        n=gen_trace(trace,MAX_TRACE);
        int taken=0; for(int i=0;i<n;i++) if(trace[i].taken) taken++;
        printf("Synthetic trace: %d branches (%d taken, %d not-taken)\n",n,taken,n-taken);
    }

    printf("\n── Predictor Comparison ────────────────────────────\n");
    printf("  %-30s  %-22s  %-8s  %-8s\n","Predictor","Correct/Total","Accuracy","MPKI");
    printf("  %s\n","----------------------------------------------------------------------");

    predict_static(trace,n);
    predict_1bit(trace,n,64);
    predict_1bit(trace,n,256);
    predict_2bit(trace,n,64);
    predict_2bit(trace,n,256);
    predict_gshare(trace,n,4,64);
    predict_gshare(trace,n,8,256);
    predict_gshare(trace,n,12,1024);
    /* Task 4: same 2-bit and GShare configurations as above. */
    predict_tournament(trace,n,256,12,1024,256);

    if(do_sweep) sweep(trace,n);

    printf("\n[DONE]\n");
    return 0;
}
