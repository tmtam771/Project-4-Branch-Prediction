#include "tage.h"
#include <string.h>

typedef struct {
    uint8_t tag;
    uint8_t ctr;       /* 3-bit saturating prediction counter: 0..7 */
    uint8_t useful;    /* 2-bit usefulness counter: 0..3 */
    bool valid;
} TageEntry;

typedef struct {
    TageEntry entry[TAGE_ENTRIES];
} TageTable;

static TageTable tagged[TAGE_NUM_TABLES];
static uint8_t base_ctr[TAGE_ENTRIES]; /* 2-bit base predictor */
static const unsigned history_lengths[TAGE_NUM_TABLES] = {2, 4, 8, 16};

typedef struct {
    int provider_table; /* -1 means base predictor */
    unsigned provider_index;
    bool prediction;
} TageMatch;

static uint64_t history_mask(unsigned length)
{
    return (UINT64_C(1) << length) - 1;
}

/* Fold the supplied shared GHR into an 8-bit history signature. */
static uint8_t fold_history(uint64_t history, unsigned length)
{
    uint8_t folded = 0;
    history &= history_mask(length);

    for (unsigned bit = 0; bit < length; ++bit) {
        if ((history >> bit) & 1U)
            folded ^= (uint8_t)(1U << (bit % TAGE_TAG_BITS));
    }
    return folded;
}

static unsigned table_index(BranchInput input, unsigned table_number)
{
    uint8_t folded = fold_history(input.history, history_lengths[table_number]);
    uint64_t pc_part = input.pc >> 2;
    return (unsigned)((pc_part ^ folded ^ (table_number * 0x35U))
                      & (TAGE_ENTRIES - 1U));
}

static uint8_t make_tag(BranchInput input, unsigned table_number)
{
    uint8_t folded = fold_history(input.history, history_lengths[table_number]);
    uint64_t pc_part = input.pc >> 2;
    return (uint8_t)((pc_part ^ folded ^ (table_number * 0x9dU))
                     & ((1U << TAGE_TAG_BITS) - 1U));
}

static bool counter_prediction(uint8_t counter)
{
    return counter >= 4;
}

static void train_counter(uint8_t *counter, bool actual, uint8_t maximum)
{
    if (actual) {
        if (*counter < maximum) ++(*counter);
    } else {
        if (*counter > 0) --(*counter);
    }
}


static TageMatch find_provider(BranchInput input)
{
    unsigned base_index =
        (unsigned)((input.pc >> 2) & (TAGE_ENTRIES - 1U));

    // Base predictor uses 2-bit counters
    TageMatch match = {
        -1,
        0,
        base_ctr[base_index] >= 2
    };

    /* Search from short to long history;
       the longest matching table wins. */
    for (unsigned t = 0; t < TAGE_NUM_TABLES; ++t) {
        unsigned index = table_index(input, t);
        TageEntry *entry = &tagged[t].entry[index];

        if (entry->valid &&
            entry->tag == make_tag(input, t)) {
            match.provider_table = (int)t;
            match.provider_index = index;
            match.prediction = counter_prediction(entry->ctr);
        }
    }

    return match;
}


void tage_init(void)
{
    memset(tagged, 0, sizeof(tagged));
    for (unsigned i = 0; i < TAGE_ENTRIES; ++i)
        base_ctr[i] = 2; /* weakly Taken */
}

bool tage_predict(BranchInput input)
{
    return find_provider(input).prediction;
}

void tage_update(BranchInput input, bool actual)
{
    TageMatch match = find_provider(input);
    unsigned base_index = (unsigned)((input.pc >> 2) & (TAGE_ENTRIES - 1U));

    if (match.provider_table >= 0) {
        TageEntry *provider =
            &tagged[match.provider_table].entry[match.provider_index];
        bool was_correct = (match.prediction == actual);

        train_counter(&provider->ctr, actual, 7);

        if (was_correct) {
            if (provider->useful < 3) ++provider->useful;
        } else if (provider->useful > 0) {
            --provider->useful;
        }
    } else {
        train_counter(&base_ctr[base_index], actual, 3);
    }

    /* On a miss, allocate in a table with longer history than the provider. */
    if (match.prediction != actual) {
        unsigned first_candidate = match.provider_table < 0
            ? 0U : (unsigned)match.provider_table + 1U;
        bool allocated = false;

        for (unsigned t = first_candidate;
             t < TAGE_NUM_TABLES && !allocated; ++t) {
            unsigned index = table_index(input, t);
            TageEntry *entry = &tagged[t].entry[index];

            if (!entry->valid || entry->useful == 0) {
                entry->valid = true;
                entry->tag = make_tag(input, t);
                entry->ctr = actual ? 4 : 3; /* weakly predict actual */
                entry->useful = 0;
                allocated = true;
            }
        }

        /* If no slot is available, age usefulness to permit later replacement. */
        if (!allocated) {
            for (unsigned t = first_candidate; t < TAGE_NUM_TABLES; ++t) {
                unsigned index = table_index(input, t);
                TageEntry *entry = &tagged[t].entry[index];
                if (entry->useful > 0) --entry->useful;
            }
        }
    }
}

uint64_t tage_storage_bits(void)
{
    /* 4 * 256 * (8 tag + 3 prediction + 2 useful + 1 valid) + 256 * 2 base. */
    const uint64_t bits_per_tagged_entry =
        TAGE_TAG_BITS + TAGE_CTR_BITS + TAGE_U_BITS + 1U;

    return (uint64_t)TAGE_NUM_TABLES * TAGE_ENTRIES * bits_per_tagged_entry
           + (uint64_t)TAGE_ENTRIES * 2U;
}
