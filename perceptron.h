#ifndef PERCEPTRON_H
#define PERCEPTRON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PERCEPTRON_HISTORY_LEN   32     
#define PERCEPTRON_TABLE_SIZE    128    
#define PERCEPTRON_THETA         75     
#define PERCEPTRON_MAX_WEIGHT    127    
#define PERCEPTRON_MIN_WEIGHT  -128    

typedef struct {
    int8_t weights[PERCEPTRON_HISTORY_LEN + 1]; 
} PerceptronEntry;

typedef struct {
    PerceptronEntry table[PERCEPTRON_TABLE_SIZE];
    uint64_t ghr; 
} PerceptronPredictor;

void perceptron_init(PerceptronPredictor *pred);
bool perceptron_predict(const PerceptronPredictor *pred, uint64_t pc, int *out_y);
void perceptron_update(PerceptronPredictor *pred, uint64_t pc, bool actual_taken, int y);
size_t perceptron_get_storage_bits(void);

#endif 