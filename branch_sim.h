#ifndef BRANCH_SIM_H
#define BRANCH_SIM_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TRACE 500000
#define MAX_TABLE 4096
#define MAX_PREDICTORS 10

typedef struct { uint64_t pc; bool taken; } Branch;
typedef enum { ALWAYS_T, ALWAYS_N, ONE_BIT, TWO_BIT, GSHARE, TOURNEY_LAB4 } Kind;

typedef struct {
    Kind kind;
    char name[64];
    int size;
    int gshare_size;
    int history_bits;
    int selector_size;
    uint8_t table[MAX_TABLE];
    uint8_t gshare_table[MAX_TABLE];
    uint8_t selector[MAX_TABLE];
    uint64_t correct, mispred;
} Predictor;

/* Classic baseline predictor API */
void init_model(Predictor *p, Kind kind, const char *name,
                int size, int hbits, int gsize, int selsize);
bool predict(const Predictor *p, uint64_t pc, uint64_t history);
void update(Predictor *p, uint64_t pc, uint64_t history, bool actual);
uint64_t storage_bits(const Predictor *p);

/* Trace API */
int built_in_trace(Branch *out, int max);
int load_trace(const char *path, Branch *out, int max);

#endif
