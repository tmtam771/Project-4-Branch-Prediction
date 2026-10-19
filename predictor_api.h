
#ifndef PREDICTOR_API_H
#define PREDICTOR_API_H

#include <stdint.h>
#include <stdbool.h>

/* Shared input for advanced predictors */
typedef struct {
    uint64_t pc;
    uint64_t history;
} BranchInput;

/* TAGE Predictor - Member 2 */
void tage_init(void);

bool tage_predict(BranchInput input);

void tage_update(BranchInput input, bool actual);

uint64_t tage_storage_bits(void);

/* Perceptron Predictor - Member 3 */
void perceptron_init(void);

bool perceptron_predict(BranchInput input);

void perceptron_update(BranchInput input, bool actual);

uint64_t perceptron_storage_bits(void);

#endif
