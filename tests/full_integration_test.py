"""Project 4 end-to-end integration tests (standard Python, no pytest needed).

From the repository root:
    python tests/full_integration_test.py
    python tests/full_integration_test.py --stress

Tests BOTH binary configurations without modifying the repository's source files.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ["branch_sim.c", "predictors.c", "trace.c", "tage.c", "tournament.c"]
BASELINES = [
    "Always-Taken", "Always-Not-Taken", "1-bit (size=64)",
    "1-bit (size=256)", "2-bit (size=64)", "2-bit (size=256)",
    "GShare (h=4,size=64)", "GShare (h=8,size=256)",
    "GShare (h=12,size=1024)", "Lab4 Tournament (1-bit)",
]
ROW = re.compile(
    r"^\s*(.+?)\s+(\d+)/(\d+)\s+([\d.]+)%\s+([\d.]+)\s+"
    r"(N/A|[\d.]+)\s+(\d+)\s*$"
)


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def command(args, *, expected_success=True):
    result = subprocess.run(
        [str(a) for a in args], cwd=ROOT, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        encoding="utf-8", errors="replace", timeout=120,
    )
    if expected_success and result.returncode != 0:
        raise RuntimeError(
            f"Command FAILED ({result.returncode}): {' '.join(map(str, args))}\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    if not expected_success and result.returncode == 0:
        raise AssertionError(f"Expected rejection, but command succeeded: {args}")
    return result.stdout


def build(exe, perceptron):
    args = ["gcc", "-std=c11", "-Wall", "-Wextra", "-O2"]
    if perceptron:
        args.append("-DBRANCH_SIM_USE_PERCEPTRON")
    args += ["-o", str(exe), *SOURCES]
    if perceptron:
        args.append("perceptron.c")
    command(args)


def parse(output):
    rows = {}
    for line in output.splitlines():
        match = ROW.fullmatch(line)
        if match:
            name, correct, total, accuracy, mpkb, mpki, bits = match.groups()
            check(name not in rows, f"Duplicate result: {name}")
            rows[name] = {
                "correct": int(correct), "total": int(total),
                "acc": float(accuracy), "mpkb": float(mpkb),
                "mpki": None if mpki == "N/A" else float(mpki),
                "bits": int(bits),
            }
    check(rows, "Could not parse predictor result table")
    check("[DONE]" in output, "Simulator did not finish normally")
    return rows


def run_sim(exe, *args):
    return command([exe, *args])


def validate_math(rows, n, instructions=None):
    for name, row in rows.items():
        c = row["correct"]
        check(row["total"] == n, f"{name}: incorrect branch count")
        check(0 <= c <= n, f"{name}: impossible correct count")
        mispred = n - c
        check(abs(row["acc"] - 100*c/n) <= 0.011,
              f"{name}: accuracy calculation differs")
        check(abs(row["mpkb"] - 1000*mispred/n) <= 0.011,
              f"{name}: MPKB calculation differs")
        if instructions is None:
            check(row["mpki"] is None, f"{name}: expected MPKI=N/A")
        else:
            check(row["mpki"] is not None, f"{name}: expected numeric MPKI")
            check(abs(row["mpki"] - 1000*mispred/instructions) <= 0.011,
                  f"{name}: MPKI calculation differs")


def run_unit_tests(directory):
    for name, src in [
        ("TAGE", "tage.c"), ("Perceptron", "perceptron.c"),
        ("Tournament", "tournament.c"),
    ]:
        test = ROOT / "tests" / f"test_{name.lower()}.c"
        check(test.exists(), f"Missing unit test: {test}")
        exe = directory / (f"unit_{name.lower()}" + (".exe" if sys.platform == "win32" else ""))
        command(["gcc", "-std=c11", "-Wall", "-Wextra", "-O2",
                 "-I.", "-o", exe, str(test), src])
        output = command([exe])
        check("PASS" in output, f"{name} unit test did not print PASS")
    print("PASS  Unit tests: TAGE, Perceptron, Tournament")


def run_suite(stress=False):
    with tempfile.TemporaryDirectory(prefix="p4_integration_") as temp:
        work = Path(temp)
        suffix = ".exe" if sys.platform == "win32" else ""
        perc = work / ("sim_perceptron" + suffix)
        gshare = work / ("sim_gshare" + suffix)
        build(perc, True)
        build(gshare, False)
        print("PASS  Build: TAGE+Perceptron and TAGE+GShare")

        output_p = run_sim(perc)
        output_g = run_sim(gshare)
        rows_p, rows_g = parse(output_p), parse(output_g)
        check("branches=580" in output_p and "branches=580" in output_g,
              "Unexpected built-in trace length")
        validate_math(rows_p, 580)
        validate_math(rows_g, 580)
        check(rows_p["Always-Taken"]["correct"] == 390,
              "Classic Always-Taken regression")
        check(rows_p["GShare (h=12,size=1024)"]["correct"] == 474,
              "Classic GShare regression")
        check("Perceptron" in rows_p and
              "Tournament (TAGE+Perceptron)" in rows_p,
              "Perceptron configuration missing predictor rows")
        check("Tournament (TAGE+GShare)" in rows_g,
              "GShare configuration missing Tournament row")
        check("Perceptron" not in rows_g, "GShare-only build leaked Perceptron")
        for name in [*BASELINES, "TAGE"]:
            check(rows_p[name] == rows_g[name],
                  f"Cross-build regression in {name}")
        print("PASS  Built-in trace: baselines and two Tournament configurations")

        check(rows_p["TAGE"]["bits"] == 14880, "TAGE storage differs")
        check(rows_p["Perceptron"]["bits"] == 33824, "Perceptron storage differs")
        check(rows_p["Tournament (TAGE+Perceptron)"]["bits"] == 50720,
              "TAGE+Perceptron storage differs")
        check(rows_g["Tournament (TAGE+GShare)"]["bits"] == 18976,
              "TAGE+GShare storage differs")
        print("PASS  Storage accounting: shared GHR and Tournament chooser")

        check(output_p == run_sim(perc), "Perceptron build is not deterministic")
        check(output_g == run_sim(gshare), "GShare build is not deterministic")
        print("PASS  Determinism: identical trace yields identical output")

        custom = "traces/my_trace.txt"
        custom_p = run_sim(perc, custom)
        custom_g = run_sim(gshare, custom)
        check("branches=280" in custom_p and "branches=280" in custom_g,
              "Custom trace branch count changed")
        cp, cg = parse(custom_p), parse(custom_g)
        validate_math(cp, 280)
        validate_math(cg, 280)
        for name in [*BASELINES, "TAGE"]:
            check(cp[name] == cg[name], f"Custom trace mismatch in {name}")
        print("PASS  Custom trace: 280 branches and cross-config invariants")

        # 5,800 is an artificial instruction count for testing the formula.
        # Do NOT report these as measured benchmark MPKI values.
        instr = 5800
        mpki_p = parse(run_sim(perc, "--instructions", str(instr)))
        mpki_g = parse(run_sim(gshare, "--instructions", str(instr)))
        validate_math(mpki_p, 580, instr)
        validate_math(mpki_g, 580, instr)
        print("PASS  MPKI formula: synthetic denominator; separate from MPKB")

        bad = work / "invalid_trace.txt"
        bad.write_text("0x100 X\n", encoding="utf-8")
        command([perc, bad], expected_success=False)
        command([perc, "--instructions", "579"], expected_success=False)
        command([perc, work / "does_not_exist.txt"], expected_success=False)
        print("PASS  Input validation: malformed/missing trace, invalid count")

        medium = work / "medium_trace.txt"
        with medium.open("w", encoding="utf-8") as f:
            for i in range(12000):
                pc = 0x100 + (i % 37) * 4
                outcome = "T" if (i * 13 + i // 11) % 7 < 4 else "N"
                f.write(f"0x{pc:x} {outcome}\n")
        med_p, med_g = parse(run_sim(perc, medium)), parse(run_sim(gshare, medium))
        validate_math(med_p, 12000)
        validate_math(med_g, 12000)
        for name in [*BASELINES, "TAGE"]:
            check(med_p[name] == med_g[name], f"Medium trace mismatch in {name}")
        print("PASS  12,000-branch synthetic workload")

        run_unit_tests(work)

        if stress:
            too_large = work / "over_limit_trace.txt"
            with too_large.open("w", encoding="utf-8") as f:
                for i in range(500001):
                    f.write(f"0x{0x100 + (i % 7)*4:x} {'T' if i%2 else 'N'}\n")
            command([perc, too_large], expected_success=False)
            command([gshare, too_large], expected_success=False)
            print("PASS  MAX_TRACE limit: rejected 500,001 branches")

        print("\nALL FULL INTEGRATION TESTS PASSED")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stress", action="store_true",
                        help="also test the 500,000-branch input limit")
    args = parser.parse_args()
    try:
        run_suite(stress=args.stress)
    except (AssertionError, RuntimeError, OSError, subprocess.TimeoutExpired,
            KeyError, ValueError) as exc:
        print(f"\nFAIL  {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
