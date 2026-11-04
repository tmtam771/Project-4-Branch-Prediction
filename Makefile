CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
SRC = branch_sim.c predictors.c trace.c

all: branch_sim

branch_sim: $(SRC) branch_sim.h
	$(CC) $(CFLAGS) -o branch_sim $(SRC)

run: branch_sim
	./branch_sim

trace: branch_sim
	./branch_sim traces/my_trace.txt

clean:
	rm -f branch_sim branch_sim.exe
