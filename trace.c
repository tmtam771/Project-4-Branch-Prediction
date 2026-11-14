/* Project 4 trace reader: legacy fixed-size API plus constant-memory streaming. */
#include "branch_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

/* Preserve Lab 4 built-in trace exactly. */
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

bool trace_stream_open(TraceStream *stream, const char *path)
{
    stream->file = fopen(path, "r");
    stream->line_number = 0;
    stream->branch_number = 0;
    if (!stream->file) {
        perror(path);
        return false;
    }
    return true;
}

void trace_stream_close(TraceStream *stream)
{
    if (stream->file) fclose(stream->file);
    stream->file = NULL;
}

/* 1 = next branch, 0 = clean EOF, -1 = invalid input / I/O error.
   This function uses only a fixed-size line buffer, no matter how long the trace. */
int trace_stream_next(TraceStream *stream, Branch *out)
{
    char line[256];
    while (fgets(line, sizeof(line), stream->file)) {
        ++stream->line_number;
        size_t len = strlen(line);
        /* Reject truncated lines instead of accidentally reading two branches. */
        if (len && line[len-1] != '\n' && !feof(stream->file)) {
            int ch;
            while ((ch = fgetc(stream->file)) != '\n' && ch != EOF) {}
            fprintf(stderr, "Trace line too long at line %" PRIu64 "\n", stream->line_number);
            return -1;
        }

        char *p = line;
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '#' || *p == '\0') continue;

        char token[64], outcome[16], extra[2];
        unsigned long long pc = 0;
        int fields = sscanf(p, "%63s %15s %1s", token, outcome, extra);
        if (fields == 1) {
            if (strlen(token) >= sizeof(outcome)) {
                fprintf(stderr, "Invalid outcome at line %" PRIu64 "\n", stream->line_number);
                return -1;
            }
            strcpy(outcome, token);
            pc = (unsigned long long)stream->branch_number; /* Legacy bare T/N format */
        } else if (fields == 2) {
            errno = 0;
            char *end = NULL;
            if (token[0] == '-') {
                fprintf(stderr, "Invalid PC at line %" PRIu64 "\n", stream->line_number);
                return -1;
            }
            pc = strtoull(token, &end, 0);
            if (errno || !token[0] || end == token || *end) {
                fprintf(stderr, "Invalid PC at line %" PRIu64 "\n", stream->line_number);
                return -1;
            }
        } else {
            fprintf(stderr, "Invalid trace format at line %" PRIu64 "\n", stream->line_number);
            return -1;
        }

        if (strcmp(outcome, "T") && strcmp(outcome, "t") && strcmp(outcome, "1") &&
            strcmp(outcome, "N") && strcmp(outcome, "n") && strcmp(outcome, "0")) {
            fprintf(stderr, "Invalid outcome at line %" PRIu64 " (use T or N)\n", stream->line_number);
            return -1;
        }
        *out = (Branch){(uint64_t)pc, outcome[0]=='T'||outcome[0]=='t'||outcome[0]=='1'};
        ++stream->branch_number;
        return 1;
    }
    if (ferror(stream->file)) {
        perror("Error reading trace");
        return -1;
    }
    return 0;
}

/* Keep the old API for legacy callers, with its original max limit. */
int load_trace(const char *path, Branch *out, int max)
{
    TraceStream stream;
    if (!trace_stream_open(&stream, path)) return -1;
    int n = 0, status;
    Branch next;
    while ((status = trace_stream_next(&stream, &next)) == 1) {
        if (n >= max) {
            fprintf(stderr, "Trace exceeds legacy load_trace limit (%d)\n", max);
            trace_stream_close(&stream);
            return -1;
        }
        out[n++] = next;
    }
    trace_stream_close(&stream);
    return status < 0 ? -1 : n;
}
