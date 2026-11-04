#include "tage.h"
#include <stdio.h>

int main(void)
{
    tage_init();

    BranchInput input = {
        .pc = 0x100,
        .history = 0
    };

    /* Initial base counter = 2 (Weakly Taken) */
    if (!tage_predict(input)) {
        printf("FAIL: Base predictor should predict Taken\n");
        return 1;
    }

    /* Check theoretical hardware storage */
    if (tage_storage_bits() != 14848ULL) {
        printf("FAIL: Incorrect TAGE storage bits\n");
        return 1;
    }

    printf("PASS: TAGE initialization and storage\n");
    return 0;
}
