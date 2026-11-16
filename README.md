# Project 4 — Branch Prediction Simulator

A modular C branch-prediction simulator comparing classic baselines with TAGE, a perceptron predictor, and a 2-bit tournament selector. The simulator uses one shared global history register (GHR) and supports streaming **text branch traces** without buffering the whole input file.

## Implemented predictors

- **Classic baselines:** Always-Taken, Always-Not-Taken, 1-bit, 2-bit, and GShare (several configurations).
- **Lab 4 Tournament:** legacy 1-bit-selector baseline (kept for regression comparison).
- **TAGE:** four tagged tables with history lengths 2, 4, 8 and 16, plus a base predictor and simplified alternate prediction.
- **Perceptron:** 128 entries, 32-bit history, signed 8-bit weights, and threshold-based learning.
- **Project 4 Tournament:** 2-bit chooser; build either TAGE + Perceptron or TAGE + GShare.

All predictors predict using the same pre-update branch history. Their state is updated before the shared GHR advances **once per branch**.

## Requirements

- GCC with C11 support
- Python 3 (standard library; no extra Python packages required)
- Optional: GNU Make, for the Makefile targets

## Build and run (Windows PowerShell)

From the repository root, compile **TAGE + Perceptron**:

```powershell
gcc -std=c11 -Wall -Wextra -O2 -DBRANCH_SIM_USE_PERCEPTRON -o branch_sim.exe branch_sim.c predictors.c trace.c tage.c tournament.c perceptron.c
.\branch_sim.exe
.\branch_sim.exe .\traces\my_trace.txt
```

Compile **TAGE + GShare**:

```powershell
gcc -std=c11 -Wall -Wextra -O2 -o branch_sim_gshare.exe branch_sim.c predictors.c trace.c tage.c tournament.c
.\branch_sim_gshare.exe
.\branch_sim_gshare.exe .\traces\my_trace.txt
```

If you have GNU Make, the equivalent targets are `make` and `make branch_sim_gshare`.

## Build and run (Linux / WSL)

```bash
make
./branch_sim
make branch_sim_gshare
./branch_sim_gshare traces/my_trace.txt
```

## Trace format and streaming

A text trace contains one branch per line:

```text
# PC  Outcome
0x100 T
0x104 N
0x108 T
```

`T` means taken; `N` means not taken. The reader also accepts `t/n`, `1/0`, and legacy lines containing only the outcome. Blank lines and `#` comments are skipped.

The main simulator streams file traces one branch at a time, **preserving predictor state and GHR across the complete file**. Its legacy `load_trace()` function retains an older fixed-size limit but is not used by the streaming main simulation loop. This implementation does **not** directly parse compressed/binary ChampSim traces; those must first be converted into a compatible, validated text branch trace.

## Metrics

The simulator reports prediction accuracy, mispredictions per 1,000 **branches** (MPKB), mispredictions per 1,000 **instructions** (MPKI), and theoretical predictor storage (bits):

- `Accuracy (%) = 100 * correct_predictions / total_branches`
- `MPKB = 1000 * mispredictions / total_branches`
- `MPKI = 1000 * mispredictions / total_instructions`

**MPKB is not MPKI.** Branch-only traces do not include a verified instruction count. Without that metadata, the simulator correctly reports `MPKI=N/A`. Only pass an instruction count that truly corresponds to the same trace window:

```powershell
.\branch_sim.exe .\traces\my_trace.txt --instructions VERIFIED_COUNT
```

Replace `VERIFIED_COUNT` with the actual positive instruction count (not the literal text). Do not use arbitrary values for published benchmark results. Storage accounting includes the shared 32-bit GHR only once within each combined predictor configuration.

## Tests

From the repository root:

```powershell
python .\tests\smoke_test.py
python .\tests\full_integration_test.py
python .\tests\full_integration_test.py --stress
```

The integration suite builds both tournament variants, validates Lab 4 baseline regression, compares configurations, checks metrics and storage, tests invalid input, runs independent predictor unit tests, and verifies that streaming processes a 500,001-branch synthetic trace. GitHub Actions is configured through `.github/workflows/full-integration.yml` to run the stress-enabled suite on pushes and pull requests to `main`.

Passing these tests validates the covered behavior; it does not establish accuracy or correctness on every possible workload.

## Repository layout

```text
branch_sim.c            Main simulator, shared GHR, metrics, streaming loop
branch_sim.h            Classic API, trace stream interface, shared definitions
predictor_api.h         Shared TAGE / Perceptron interface
predictors.c            Classic predictors and legacy Lab 4 tournament
tage.c, tage.h          TAGE predictor
perceptron.c, .h        Perceptron predictor
tournament.c, .h        Project 4 two-bit chooser
trace.c                 Built-in trace and text-stream parser
Makefile                Build targets
tests/                  Smoke, unit, and full integration tests
traces/my_trace.txt     Small custom branch trace
.github/workflows/      CI integration testing
```

## Benchmark evaluation — remaining work

To complete the final evaluation, collect at least four suitable workloads (integer, floating-point, server and graph, if required by the project specification), record trace provenance and verified instruction counts, and run every predictor configuration on equivalent trace windows. Report accuracy, **true MPKI** when available, and storage bits; prepare comparison charts and an MPKI-versus-storage Pareto analysis.

The repository's built-in and custom traces are for regression/debugging and are **not substitutes for real benchmark results**. Document any trace conversion, warm-up policy, sampling limits, and limitations in the final report.

## Team responsibilities

- Member 1: framework, shared APIs, integration, CI, streaming and metric validation.
- Member 2: TAGE implementation and predictor-specific tests.
- Member 3: perceptron implementation and predictor-specific tests.
- Member 4: 2-bit tournament selector and integration support.
- Member 5: workload preparation, benchmark runs, result tables and plots.
