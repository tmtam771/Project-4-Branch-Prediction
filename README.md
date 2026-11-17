# Project 4 — Branch Prediction Simulator

A modular C branch-prediction simulator comparing classic baselines with TAGE, a perceptron predictor, and a 2-bit tournament selector. The simulator uses one shared global history register (GHR) and supports streaming **text branch traces** without buffering the whole input file.

## Project status and quick links

**Implementation and benchmark evaluation are available on `main` (17 November 2026).** The framework, advanced predictors, 6 converted workloads, 12 simulator result logs, 6 converter verification logs, CSV summary, and both charts are committed. The 4–5-page final paper, presentation slides, and live-demo rehearsal remain team deliverables; they are not currently stored in this repository.

- [Benchmark summary CSV](results/benchmark_results.csv) | [CSV generator](generate_benchmark_csv.py)
- [Figure 1 — MPKI comparison](figures/fig1_mpki_comparison.png) | [Figure 2 — MPKI vs. storage / Pareto](figures/fig2_pareto_frontier.png)
- [Benchmark results and verification logs](results/) | [Converted branch traces](traces/) | [Integration tests](tests/)
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
.\branch_sim.exe .\traces\gcc.txt --instructions 3134864
```

The GCC example uses its verified **3,134,864** instructions for exactly the 600,000 converted branches. Use the matching verified count from each trace's `# Verified instructions:` header for other workloads; never invent a count. Storage accounting includes the shared 32-bit GHR only once within each combined predictor configuration.

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
branch_sim.c, branch_sim.h   Shared harness, metrics, streaming, GHR
predictor_api.h              Shared interface for advanced predictors
predictors.c                 Classic baselines and Lab 4 tournament
tage.c, tage.h               Simplified TAGE implementation
perceptron.c, perceptron.h   Perceptron implementation
tournament.c, tournament.h   Project 4 two-bit selector
trace.c                      Text trace and streaming reader
convert_trace.py             Convert ChampSim .xz/.gz to validated .txt
generate_benchmark_csv.py    Aggregate results and validate metrics
traces/                      Six converted benchmark traces + my_trace.txt
results/                     12 simulator logs, 6 verification logs, CSV
figures/                     MPKI comparison and storage/MPKI Pareto plots
tests/                       Smoke, unit, integration and converter checks
Makefile                     Linux / GNU Make build targets
.github/workflows/           Full-integration CI
```

## Benchmark evaluation — completed

### Workloads and trace validation

The evaluation uses six **ChampSim-derived** workloads spanning four categories. Each converted text trace contains the **first 600,000 recorded branches**, with a matching independently checked instruction count:

| Workload | Category | Branches | Verified instructions |
|---|---|---:|---:|
| GCC | Integer | 600,000 | 3,134,864 |
| MCF | Integer | 600,000 | 6,838,985 |
| BWAVES | Floating-point | 600,000 | 5,525,498 |
| XALANCBMK | Integer | 600,000 | 2,297,562 |
| Server (Node.js) | Server | 600,000 | 3,116,450 |
| Graph (Ligra PageRankDelta) | Graph | 600,000 | 1,576,476 |

The converter supports standard **64-byte, little-endian ChampSim `input_instr`** records in both `.xz` and `.gz` compression. For each workload, `results/<workload>-verification.txt` documents the comparison against the original compressed source: branch PC, direction, ordering, and instruction count agree **through the last converted branch**. Validation covers that prefix, **not** the entire original trace. The original large compressed traces are not required to run the already converted text traces and are excluded by `.gitignore`. For provenance, see the `# Source:` line in each converted trace and the corresponding verification log.

**Experimental scope:** no separate predictor warm-up is excluded; statistics include initial training on the selected trace prefix. This is a branch-direction predictor study, not an IPC, power, timing, full-CPU or speculative-execution evaluation. The TAGE model is simplified, not production TAGE.

### Results and reproducibility

Two builds evaluate the same workload windows:

- **TAGE + Perceptron build:** `results/<workload>-perceptron.txt` — includes standalone GShare, TAGE, Perceptron, and Tournament (TAGE+Perceptron).
- **TAGE + GShare build:** `results/<workload>-gshare.txt` — includes standalone GShare, TAGE, and Tournament (TAGE+GShare).

Five reported configurations across six workloads yield **30 rows** in [results/benchmark_results.csv](results/benchmark_results.csv). Both builds also report classic baseline models.

| Workload | GShare | TAGE | Perceptron | TAGE + Perceptron | TAGE + GShare |
|---|---:|---:|---:|---:|---:|
| GCC | 0.97 | 6.08 | **0.52** | 0.55 | 0.86 |
| MCF | 1.62 | 2.96 | **1.58** | 1.58 | 1.61 |
| BWAVES | 7.05 | 9.29 | 6.20 | **6.17** | 7.02 |
| XALANCBMK | 8.81 | 9.68 | **3.86** | 3.88 | 5.20 |
| Server | 50.13 | 64.88 | **29.53** | 30.35 | 47.42 |
| Graph | 23.86 | 23.77 | 0.11 | **0.10** | 23.77 |

*Values are MPKI (lower is better), rounded to two decimals. On MCF the two 1.58 values differ in unrounded mispredictions; Perceptron has fewer errors.*

Logical predictor storage reported by the simulator (bits): **GShare 2,060; TAGE 14,880; TAGE+GShare 18,976; Perceptron 33,824; TAGE+Perceptron 50,720**. This accounting includes the shared GHR once, and it is **not** measured chip area or power.

Generate and independently validate the summary CSV from the existing trace metadata and simulator logs:

```powershell
python .\generate_benchmark_csv.py
```

Expected summary: `PASS: Saved 30 results (6 workloads x 5 predictors)`; output: `results/benchmark_results.csv`.

To reproduce an individual benchmark (Windows PowerShell), first build the desired executable as described above, then pass the matching verified instruction count:

```powershell
.\branch_sim.exe .\traces\gcc.txt --instructions 3134864
.\branch_sim_gshare.exe .\traces\gcc.txt --instructions 3134864
```

See each workload's `traces/<workload>.txt` metadata for its exact count. For converter verification, the original `.xz`/`.gz` file must be available locally; it is not committed. The independent verifier is `tests/verify_champsim_converter.py`.

### Figures

- **[Figure 1: MPKI comparison — 5 predictor configurations × 6 workloads](figures/fig1_mpki_comparison.png)**
- **[Figure 2: Pareto frontier — hardware storage vs. MPKI for each workload](figures/fig2_pareto_frontier.png)**

Lower MPKI and lower storage are preferable. The Pareto frontier is workload-specific: TAGE+Perceptron is non-dominated on **BWAVES and Graph** only, while GShare remains the smallest tested design.

### Testing and CI notes

`python tests/full_integration_test.py --stress` runs in [GitHub Actions](.github/workflows/full-integration.yml) for pushes and pull requests to `main`. This tests simulator behavior, metrics, baseline regressions, and streaming; it does **not** automatically re-download or verify the original external compressed traces, nor does a green CI run alone validate all benchmark conclusions.

## Remaining team deliverables

- Final **4–5-page paper** covering predictor comparison, theoretical hardware-cost trade-offs, **security discussion**, limitations, and references.
- **15-minute presentation**, including a prepared **live trace analysis** showing the simulator running and its output being interpreted.
- Add the reviewed paper/slides to the repository when finalized; neither artifact is presently committed.

## Team responsibilities

- Member 1: framework, shared APIs, integration, CI, streaming and metric validation.
- Member 2: TAGE implementation and predictor-specific tests.
- Member 3: perceptron implementation and predictor-specific tests.
- Member 4: 2-bit tournament selector and integration support.
- Member 5: workload preparation, benchmark runs, result tables and plots.
