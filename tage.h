#ifndef TAGE_H
#define TAGE_H

#include "predictor_api.h"

/* Initial Project 4 configuration; values can be changed if the team specifies otherwise. */
#define TAGE_NUM_TABLES 4
#define TAGE_ENTRIES 256
#define TAGE_TAG_BITS 8
#define TAGE_CTR_BITS 3
#define TAGE_U_BITS 2

void tage_init(void);
bool tage_predict(BranchInput input);
void tage_update(BranchInput input, bool actual);
uint64_t tage_storage_bits(void);

#endif
