/* Classic predictors ported from Lab 4. */
#include "branch_sim.h"
#include <stdio.h>
#include <string.h>

void init_model(Predictor *p, Kind kind, const char *name,
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

    if (kind == PERCEPTRON) {
        perceptron_init(&p->perc);
        p->last_y = 0;
    }
}

static uint32_t idx_pc(uint64_t pc, int size) {
    return (uint32_t)(pc % (uint64_t)size);
}
static uint32_t idx_gshare(uint64_t pc, uint64_t history, int bits, int size) {
    uint64_t mask = (UINT64_C(1) << bits) - 1;
    return (uint32_t)((pc ^ (history & mask)) % (uint64_t)size);
}
bool predict(const Predictor *p, uint64_t pc, uint64_t history) {
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

    case PERCEPTRON:
        return perceptron_predict(&p->perc, pc, (int *)&((Predictor *)p)->last_y);
    }
    return false;
}
static void train_2bit(uint8_t *ctr, bool actual) {
    if (actual && *ctr < 3) ++*ctr;
    else if (!actual && *ctr > 0) --*ctr;
}
void update(Predictor *p, uint64_t pc, uint64_t history, bool actual) {
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
    case PERCEPTRON:
        perceptron_update(&p->perc, pc, actual, p->last_y);     
        return;
    }
}
uint64_t storage_bits(const Predictor *p) {
    switch(p->kind) {
    case ALWAYS_T: case ALWAYS_N: return 0;
    case ONE_BIT: return (uint64_t)p->size;
    case TWO_BIT: return 2ULL * p->size;
    case GSHARE: return 2ULL * p->size + p->history_bits;
    case TOURNEY_LAB4:
        return 2ULL*p->size + 2ULL*p->gshare_size + p->history_bits + p->selector_size;
    case PERCEPTRON:
        return perceptron_get_storage_bits();
    }
    return 0;
}