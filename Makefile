CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
SRC = branch_sim.c predictors.c trace.c tage.c tournament.c
HEADERS = branch_sim.h predictor_api.h tage.h tournament.h

ifneq ($(wildcard perceptron.c),)
CPPFLAGS += -DBRANCH_SIM_USE_PERCEPTRON
SRC += perceptron.c
HEADERS += $(wildcard perceptron.h)
endif

all: branch_sim

branch_sim: $(SRC) $(HEADERS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -o branch_sim $(SRC)

run: branch_sim
	./branch_sim

trace: branch_sim
	./branch_sim traces/my_trace.txt

clean:
	rm -f branch_sim branch_sim.exe
