
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

COMMON = branch_sim.c predictors.c trace.c tage.c tournament.c
HEADERS = branch_sim.h predictor_api.h tage.h tournament.h perceptron.h

all: branch_sim

branch_sim: $(COMMON) perceptron.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -DBRANCH_SIM_USE_PERCEPTRON -o branch_sim $(COMMON) perceptron.c

branch_sim_gshare: $(COMMON) $(HEADERS) Makefile
	$(CC) $(CFLAGS) -o branch_sim_gshare $(COMMON)

run: branch_sim
	./branch_sim

clean:
	rm -f branch_sim branch_sim.exe branch_sim_gshare branch_sim_gshare.exe
