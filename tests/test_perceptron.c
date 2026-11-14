#include "perceptron.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    const BranchInput in = {0x100,0};
    perceptron_init();
    assert(perceptron_predict(in));
    assert(perceptron_storage_bits() == 33792ULL);
    perceptron_update(in,false);
    assert(!perceptron_predict(in));
    perceptron_init();
    assert(perceptron_predict(in));
    puts("PASS: Perceptron shared API and training");
    return 0;
}
