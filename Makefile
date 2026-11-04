CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

SRC = branch_sim.c predictors.c trace.c tage.c tournament.c
HEADERS = branch_sim.h predictor_api.h tage.h tournament.h

all: branch_sim

branch_sim: $(SRC) $(HEADERS) Makefile
	$(CC) $(CFLAGS) -o branch_sim $(SRC)

run: branch_sim
	./branch_sim

trace: branch_sim
	./branch_sim traces/my_trace.txt

clean:
	rm -f branch_sim branch_sim.exe
