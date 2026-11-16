#!/usr/bin/env python3
r"""Generate a 30-row Project 4 benchmark CSV from the verified local trace and result files.

Run from Project 4 repository root:
    python .\generate_benchmark_csv.py

Or:
    python generate_benchmark_csv.py --root . --output results/benchmark_results.csv

Requires Python standard library only. Does not change predictor source files.
"""
import argparse
import csv
import re
from pathlib import Path

WORKLOADS = {
    "gcc": "Integer",
    "mcf": "Integer",
    "bwaves": "Floating-point",
    "xalancbmk": "Integer",
    "server": "Server",
    "graph": "Graph",
}

# Names must match the current simulator exactly.
PREDICTORS = [
    ("GShare (h=12,size=1024)", "perceptron"),
    ("TAGE", "perceptron"),
    ("Perceptron", "perceptron"),
    ("Tournament (TAGE+Perceptron)", "perceptron"),
    ("Tournament (TAGE+GShare)", "gshare"),
]

RESULT_LINE = re.compile(
    r"^\s*(.+?)\s+(\d+)/(\d+)\s+([\d.]+)%\s+([\d.]+)\s+"
    r"(N/A|[\d.]+)\s+(\d+)\s*$"
)
INSTRUCTIONS_LINE = re.compile(r"^# Verified instructions:\s*(\d+)\s*$")
BRANCHES_LINE = re.compile(r"^# Branches:\s*(\d+)\s*$")
REPO_URL = "https://github.com/tmtam771/Project-4-Branch-Prediction/blob/main/"


def load_trace_metadata(root, workload):
    p = root / "traces" / f"{workload}.txt"
    if not p.is_file():
        raise FileNotFoundError(f"Missing trace: {p}")
    with p.open(encoding="utf-8-sig") as f:
        header = [f.readline().strip() for _ in range(3)]
    b = BRANCHES_LINE.fullmatch(header[1])
    i = INSTRUCTIONS_LINE.fullmatch(header[2])
    if not b or not i:
        raise ValueError(f"Missing or unexpected trace metadata in {p}: {header}")
    branches, instructions = int(b.group(1)), int(i.group(1))
    if not (0 < branches <= instructions):
        raise ValueError(f"Invalid metadata in {p}: {branches} branches, {instructions} instructions")
    return branches, instructions


def load_results(root, workload, kind):
    rel = Path("results") / f"{workload}-{kind}.txt"
    path = root / rel
    if not path.is_file():
        raise FileNotFoundError(f"Missing simulator result: {path}")
    data = path.read_text(encoding="utf-8-sig")
    if "[DONE]" not in data:
        raise ValueError(f"Simulator did not finish in {path}")
    results = {}
    for line in data.splitlines():
        m = RESULT_LINE.fullmatch(line)
        if not m:
            continue
        name, correct, n, acc, mpkb, mpki, bits = m.groups()
        if name in results:
            raise ValueError(f"Duplicate predictor {name!r} in {path}")
        results[name] = {
            "correct": int(correct), "branches": int(n),
            "acc": float(acc), "mpkb": float(mpkb),
            "mpki": None if mpki == "N/A" else float(mpki),
            "storage": int(bits),
        }
    return results, rel.as_posix()


def generate(root, output):
    rows = []
    for workload, category in WORKLOADS.items():
        n, instructions = load_trace_metadata(root, workload)
        perc, perc_path = load_results(root, workload, "perceptron")
        gshare, gshare_path = load_results(root, workload, "gshare")
        for shared in ("GShare (h=12,size=1024)", "TAGE"):
            if perc.get(shared) != gshare.get(shared):
                raise ValueError(f"Cross-configuration mismatch: {workload} / {shared}")
        for predictor, kind in PREDICTORS:
            results, rel_path = (perc, perc_path) if kind == "perceptron" else (gshare, gshare_path)
            if predictor not in results:
                raise ValueError(f"Missing {predictor!r} in {rel_path}")
            item = results[predictor]
            correct = item["correct"]
            if item["branches"] != n or not 0 <= correct <= n:
                raise ValueError(f"Bad branch/correct count in {rel_path} for {predictor}")
            misses = n - correct
            accuracy = 100 * correct / n
            mpkb = 1000 * misses / n
            mpki = 1000 * misses / instructions
            if (item["mpki"] is None or
                abs(accuracy - item["acc"]) > 0.011 or
                abs(mpkb - item["mpkb"]) > 0.011 or
                abs(mpki - item["mpki"]) > 0.011):
                raise ValueError(f"Reported metrics do not match computed ones: {rel_path} / {predictor}")
            rows.append({
                "Workload": workload.upper() if workload not in ("server", "graph") else workload.title(),
                "Category": category,
                "Predictor": predictor,
                "Branches": n,
                "Instructions": instructions,
                "Correct": correct,
                "Mispredictions": misses,
                "Accuracy_percent": f"{accuracy:.2f}",
                "MPKB": f"{mpkb:.2f}",
                "MPKI": f"{mpki:.2f}",
                "Storage_bits": item["storage"],
                "Source_URL": REPO_URL + rel_path,
            })
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"PASS: Saved {len(rows)} results ({len(WORKLOADS)} workloads x {len(PREDICTORS)} predictors)")
    print(f"CSV: {output.resolve()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."), help="Project 4 repository root")
    parser.add_argument("--output", type=Path, default=Path("results/benchmark_results.csv"))
    args = parser.parse_args()
    try:
        generate(args.root, args.root / args.output)
    except (FileNotFoundError, ValueError) as exc:
        parser.exit(1, f"FAIL: {exc}\n")


if __name__ == "__main__":
    main()
