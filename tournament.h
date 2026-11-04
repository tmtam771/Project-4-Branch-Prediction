#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <stdbool.h>
#include <stdint.h>

enum { TOURNAMENT_ENTRIES = 1024 };

typedef struct {
    uint8_t chooser[TOURNAMENT_ENTRIES];
} Tournament;

void tournament_init(Tournament *tournament);
bool tournament_predict(const Tournament *tournament, uint64_t pc,
                        bool prediction_a, bool prediction_b);
/* Pass the predictions saved before either component was updated. */
void tournament_update(Tournament *tournament, uint64_t pc,
                       bool prediction_a, bool prediction_b, bool actual);
uint64_t tournament_chooser_bits(void);

#endif
