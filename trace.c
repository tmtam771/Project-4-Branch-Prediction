/* Branch text-trace reader + original Lab 4 synthetic trace. */
#include "branch_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Keep the original Lab 4 built-in trace behavior for regression testing. */
int built_in_trace(Branch *out, int max) {
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

int load_trace(const char *path, Branch *out, int max) {
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
