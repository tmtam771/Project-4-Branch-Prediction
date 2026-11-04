# Project 4 — Phase 1, Step 2: modular simulator

Your old Phase 1 starter has been split into three `.c` files and a shared header.
No branch-prediction logic was intentionally changed.

## Visual Studio Code on Windows (PowerShell)

1. **File > Open Folder** and select this folder.
2. Open **Terminal > New Terminal**.
3. Compile **all three source files**:

```powershell
gcc -std=c11 -Wall -Wextra -O2 -o branch_sim.exe branch_sim.c predictors.c trace.c
```

4. Run:

```powershell
.\branch_sim.exe
.\branch_sim.exe .\traces\my_trace.txt
python .\tests\smoke_test.py
```

If `python` is not recognized, use `py`.

## Linux / WSL

```bash
make
./branch_sim
./branch_sim traces/my_trace.txt
python3 tests/smoke_test.py
```

## What each file does

- `branch_sim.h`: common types and function prototypes.
- `branch_sim.c`: argument parsing, shared GHR, experiment loop, statistics.
- `predictors.c`: the Lab 4 classic predictors plus the old 1-bit-selector tournament for regression reference.
- `trace.c`: synthetic trace generator and text trace parser.
- `tests/smoke_test.py`: check regression on the Lab 4 built-in and custom traces.

The **original Lab 4** tournament is only a baseline here; the final Project 4 tournament requires a separate **2-bit selector**.

**MPKB != MPKI:** built-in and branch-only traces have no instruction count. The program correctly displays `MPKI=N/A` unless you supply a verified total instruction count via `--instructions N`.

## Next steps

- Phase 2: add `tage.c` + `tage.h` with separate predictor state, preserving the common branch history.
- Phase 3: add `perceptron.c` + `perceptron.h`.
- Phase 4: add the 2-bit tournament chooser and evaluate on at least 4 real ChampSim traces (with verified instruction counts).
