# Project 4 - Phase 1 (VS Code starter)

Built from the user's Lab 4 Task 4 simulator.

## Build

Open this folder with **File > Open Folder** in VS Code. Open **Terminal > New Terminal**.

Linux/WSL/macOS with GCC:

    gcc -std=c11 -Wall -Wextra -O2 -o branch_sim branch_sim.c
    ./branch_sim
    ./branch_sim traces/my_trace.txt

Windows PowerShell with MinGW GCC:

    gcc -std=c11 -Wall -Wextra -O2 -o branch_sim.exe branch_sim.c
    .\branch_sim.exe
    .\branch_sim.exe traces/my_trace.txt

Use `gcc --version` to check your compiler. VS Code by itself is not a compiler.

## What was preserved?

- Built-in Lab 4 trace: 580 branches.
- Always-Taken, Always-Not-Taken, 1-bit, 2-bit, GShare baselines.
- Old Lab 4 tournament with 1-bit selector (for reference; NOT the Project 4 tournament).
- Original Task 4 implementation: `lab4_branch_sim_backup.c`.

## Unified runner

All models use a common predict() and update(), and share the same GHR from the runner. The runner predicts all models first, then updates, then advances GHR once.

## MPKI warning

The Lab 4 trace files list branches, not total instructions. `MPKB` means mispredictions per 1000 **branches**. Accurate `MPKI` requires verified total **instructions** from a matching trace/workload. Therefore `MPKI` is N/A until given a real count with `--instructions N`. Do not invent N.

## Future phases

Phase 2: implement TAGE 4 tagged tables (history 2/4/8/16).
Phase 3: perceptron.
Phase 4: 2-bit tournament + ChampSim evaluation.

The C loader accepts `0x400120 T`, `0x400120 N` or a bare `T` / `N`. Binary compressed ChampSim instruction traces are NOT supported yet; they require a format-aware adapter.
