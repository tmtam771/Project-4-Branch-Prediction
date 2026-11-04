#include "tournament.h"

#include <stddef.h>
#include <string.h>

enum { WEAK_A = 1, WEAK_B = 2, STRONG_B = 3 };

static size_t chooser_index(uint64_t pc)
{
    /* Use the same word-address indexing as Lab 4 and TAGE. */
    return (size_t)((pc >> 2) % TOURNAMENT_ENTRIES);
}

void tournament_init(Tournament *tournament)
{
    memset(tournament->chooser, WEAK_A, sizeof tournament->chooser);
}

bool tournament_predict(const Tournament *tournament, uint64_t pc,
                        bool prediction_a, bool prediction_b)
{
    return tournament->chooser[chooser_index(pc)] >= WEAK_B
        ? prediction_b : prediction_a;
}

void tournament_update(Tournament *tournament, uint64_t pc,
                       bool prediction_a, bool prediction_b, bool actual)
{
    if (prediction_a == prediction_b) {
        return;
    }

    uint8_t *counter = &tournament->chooser[chooser_index(pc)];
    if (prediction_b == actual && *counter < STRONG_B) {
        ++*counter;
    } else if (prediction_a == actual && *counter > 0) {
        --*counter;
    }
}

uint64_t tournament_chooser_bits(void)
{
    return 2ULL * TOURNAMENT_ENTRIES;
}
