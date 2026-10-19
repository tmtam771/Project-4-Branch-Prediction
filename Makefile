CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

all: branch_sim

branch_sim: branch_sim.c
	$(CC) $(CFLAGS) -o branch_sim branch_sim.c

run: branch_sim
	./branch_sim

trace: branch_sim
	./branch_sim traces/my_trace.txt

clean:
	rm -f branch_sim branch_sim.exe
