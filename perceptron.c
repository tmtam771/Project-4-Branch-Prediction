#include "perceptron.h"
#include <string.h>

typedef struct {
    int8_t weights[PERCEPTRON_HISTORY_LEN + 1];
} PerceptronEntry;

static PerceptronEntry table[PERCEPTRON_TABLE_SIZE];

static unsigned table_index(uint64_t pc)
{
    return (unsigned)((pc >> 2) % PERCEPTRON_TABLE_SIZE);
}

static int score(BranchInput input)
{
    const PerceptronEntry *p = &table[table_index(input.pc)];
    int y = p->weights[0];

    for (unsigned i = 0; i < PERCEPTRON_HISTORY_LEN; ++i) {
        int x = ((input.history >> i) & 1ULL) ? 1 : -1;
        y += (int)p->weights[i + 1] * x;
    }
    return y;
}

static int8_t saturate(int value)
{
    if (value > PERCEPTRON_MAX_WEIGHT) value = PERCEPTRON_MAX_WEIGHT;
    if (value < PERCEPTRON_MIN_WEIGHT) value = PERCEPTRON_MIN_WEIGHT;
    return (int8_t)value;
}

void perceptron_init(void)
{
    memset(table, 0, sizeof(table));
}

bool perceptron_predict(BranchInput input)
{
    return score(input) >= 0;
}

void perceptron_update(BranchInput input, bool actual)
{
    int y = score(input);
    int t = actual ? 1 : -1;
    int abs_y = y < 0 ? -y : y;
    if ((y >= 0) == actual && abs_y > PERCEPTRON_THETA) return;

    PerceptronEntry *p = &table[table_index(input.pc)];
    p->weights[0] = saturate((int)p->weights[0] + t);
    for (unsigned i = 0; i < PERCEPTRON_HISTORY_LEN; ++i) {
        int x = ((input.history >> i) & 1ULL) ? 1 : -1;
        p->weights[i + 1] = saturate((int)p->weights[i + 1] + t * x);
    }
}

uint64_t perceptron_storage_bits(void)
{
    /* Only weights; shared GHR is accounted for in branch_sim.c. */
    return (uint64_t)PERCEPTRON_TABLE_SIZE * (PERCEPTRON_HISTORY_LEN + 1U) * 8U;
}
