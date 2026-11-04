
#include "tournament.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    Tournament t;
    tournament_init(&t);

    uint64_t pc = 0x100;

    // A predicts Taken, B predicts Not Taken
    bool a = true;
    bool b = false;

    // Initial chooser = 1, so choose A
    assert(tournament_predict(&t, pc, a, b) == true);

    // B is correct, chooser becomes 2
    tournament_update(&t, pc, a, b, false);
    assert(tournament_predict(&t, pc, a, b) == false);

    // A is correct, chooser becomes 1
    tournament_update(&t, pc, a, b, true);
    assert(tournament_predict(&t, pc, a, b) == true);

    // When A and B agree, chooser must not change
    tournament_update(&t, pc, true, true, true);
    assert(tournament_predict(&t, pc, a, b) == true);

    assert(tournament_chooser_bits() == 2048);

    puts("PASS: Tournament chooser");
    return 0;
}
