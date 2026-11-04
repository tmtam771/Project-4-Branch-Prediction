#include "perceptron.h"
#include <string.h>

void perceptron_init(PerceptronPredictor *pred) {
    if (!pred) return;
    memset(pred, 0, sizeof(PerceptronPredictor));
}

static inline uint32_t get_index(uint64_t pc) {
    return (uint32_t)(pc % PERCEPTRON_TABLE_SIZE);
}

bool perceptron_predict(const PerceptronPredictor *pred, uint64_t pc, int *out_y) {
    if (!pred) return false;

    uint32_t idx = get_index(pc);
    const PerceptronEntry *p = &pred->table[idx];

    int y = p->weights[0]; // Start with bias weight w0
    for (int i = 0; i < PERCEPTRON_HISTORY_LEN; i++) {
        int x_i = ((pred->ghr >> i) & 1ULL) ? 1 : -1;
        y += p->weights[i + 1] * x_i;
    }

    if (out_y) {
        *out_y = y;
    }

    return (y >= 0);
}

void perceptron_update(PerceptronPredictor *pred, uint64_t pc, bool actual_taken, int y) {
    if (!pred) return;

    uint32_t idx = get_index(pc);
    PerceptronEntry *p = &pred->table[idx];

    bool predicted_taken = (y >= 0);
    int t = actual_taken ? 1 : -1;
    int abs_y = (y < 0) ? -y : y;

    if (predicted_taken != actual_taken || abs_y <= PERCEPTRON_THETA) {
        int w0 = p->weights[0] + t;
        if (w0 > PERCEPTRON_MAX_WEIGHT) w0 = PERCEPTRON_MAX_WEIGHT;
        if (w0 < PERCEPTRON_MIN_WEIGHT) w0 = PERCEPTRON_MIN_WEIGHT;
        p->weights[0] = (int8_t)w0;

        for (int i = 0; i < PERCEPTRON_HISTORY_LEN; i++) {
            int x_i = ((pred->ghr >> i) & 1ULL) ? 1 : -1;
            int w_i = p->weights[i + 1] + (t * x_i);

            if (w_i > PERCEPTRON_MAX_WEIGHT) w_i = PERCEPTRON_MAX_WEIGHT;
            if (w_i < PERCEPTRON_MIN_WEIGHT) w_i = PERCEPTRON_MIN_WEIGHT;
            p->weights[i + 1] = (int8_t)w_i;
        }
    }

    pred->ghr = ((pred->ghr << 1) | (actual_taken ? 1ULL : 0ULL)) & ((1ULL << PERCEPTRON_HISTORY_LEN) - 1ULL);
}

size_t perceptron_get_storage_bits(void) {
    size_t weight_bits = (size_t)PERCEPTRON_TABLE_SIZE * (PERCEPTRON_HISTORY_LEN + 1) * 8;
    size_t ghr_bits = PERCEPTRON_HISTORY_LEN;
    return weight_bits + ghr_bits;
}