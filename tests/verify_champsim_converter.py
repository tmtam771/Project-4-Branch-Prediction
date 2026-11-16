#!/usr/bin/env python3
"""Compare ChampSim 64-byte legacy input_instr .xz with a converted branch .txt.

Usage:
    python verify_champsim_converter.py SOURCE.champsimtrace.xz OUTPUT.txt

Checks every converted branch against the original binary record, including PC,
branch outcome, order, instruction count, and metadata. No third-party packages.
The source trace must use the standard (not CloudSuite) input_instr layout.
"""

from __future__ import annotations

import gzip
import argparse
import lzma
import re
import struct
import sys
from pathlib import Path 

RECORD_SIZE = 64
SOURCE_RE = re.compile(r"# Source: (.+)\s*$")
BRANCHES_RE = re.compile(r"# Branches: (\d+)\s*$")
INSTRUCTIONS_RE = re.compile(r"# Verified instructions: (\d+)\s*$")
TEXT_BRANCH_RE = re.compile(r"(0[xX][0-9a-fA-F]+|[0-9]+)\s+([TtNn])\s*")


class VerificationError(Exception):
    pass


def read_header(text_file):
    values = []
    for label, pattern in [
        ("Source", SOURCE_RE),
        ("Branches", BRANCHES_RE),
        ("Verified instructions", INSTRUCTIONS_RE),
    ]:
        line = text_file.readline()
        found = pattern.fullmatch(line.rstrip("\r\n"))
        if not found:
            raise VerificationError(f"Invalid '{label}' header: {line.strip()!r}")
        values.append(found.group(1))
    source, branches_str, instructions_str = values
    branches = int(branches_str)
    instructions = int(instructions_str)
    if branches <= 0 or instructions < branches:
        raise VerificationError(f"Implausible metadata: {branches} branches, {instructions} instructions")
    return source, branches, instructions


def verify(src_path: Path, text_path: Path):
    if not src_path.is_file():
        raise VerificationError(f"Original .xz not found: {src_path}")
    if not text_path.is_file():
        raise VerificationError(f"Converted .txt not found: {text_path}")

    instructions_read = branches_read = taken = 0
    with text_path.open("r", encoding="utf-8-sig", newline="") as txt:
        declared_source, declared_branches, declared_instructions = read_header(txt)
        name_in_header = declared_source.replace("\\", "/").split("/")[-1]
        if name_in_header != src_path.name:
            raise VerificationError(
                f"Source filename mismatch: header says {name_in_header!r}; "
                f"supplied file is {src_path.name!r}"
            )
        opener = gzip.open if src_path.suffix.lower() == ".gz" else lzma.open

        with opener(src_path, "rb") as xz:
            while branches_read < declared_branches:
                record = xz.read(RECORD_SIZE)
                if not record:
                    raise VerificationError(
                        f"Source ended early after {instructions_read} instructions "
                        f"and {branches_read} branches"
                    )
                if len(record) != RECORD_SIZE:
                    raise VerificationError(
                        f"Incomplete binary record at instruction {instructions_read + 1}: "
                        f"{len(record)} bytes, expected 64"
                    )

                instructions_read += 1
                pc = struct.unpack_from("<Q", record, 0)[0]
                is_branch = record[8]
                branch_taken = record[9]
                if is_branch not in (0, 1):
                    raise VerificationError(
                        f"Instruction #{instructions_read}: is_branch={is_branch}, "
                        "not 0/1. Wrong binary format, size or alignment?"
                    )
                if branch_taken not in (0, 1):
                    raise VerificationError(
                        f"Instruction #{instructions_read}: branch_taken={branch_taken}, "
                        "not 0/1. Wrong binary format, size or alignment?"
                    )
                if not is_branch:
                    continue

                branches_read += 1
                actual_line = txt.readline()
                match = TEXT_BRANCH_RE.fullmatch(actual_line.rstrip("\r\n"))
                if not match:
                    raise VerificationError(
                        f"Converted branch #{branches_read}: invalid or missing text line "
                        f"{actual_line.strip()!r}"
                    )
                text_pc = int(match.group(1), 0)
                text_taken = match.group(2).upper() == "T"
                if text_pc != pc or text_taken != bool(branch_taken):
                    expected = f"0x{pc:x} {'T' if branch_taken else 'N'}"
                    raise VerificationError(
                        f"Branch #{branches_read} (source instruction #{instructions_read}):\n"
                        f"    Expected from .xz: {expected}\n"
                        f"    Found in .txt:    {actual_line.strip()}"
                    )
                taken += branch_taken

        remainder = txt.readline()
        if remainder:
            raise VerificationError(
                f"Extra data after declared {declared_branches} branches: {remainder.strip()!r}"
            )

    if instructions_read != declared_instructions:
        raise VerificationError(
            f"Instruction count mismatch: from .xz = {instructions_read}, "
            f"header = {declared_instructions}"
        )

    print(f"PASS: {text_path.name} matches {src_path.name}")
    print(f"  Format: standard ChampSim input_instr (64 bytes, little-endian)")
    print(f"  Branches verified:   {branches_read:,}")
    print(f"  Instructions read:   {instructions_read:,}")
    print(f"  Taken / Not-Taken:   {taken:,} / {branches_read - taken:,}")
    print(f"  Branch rate:         {branches_read / instructions_read:.2%}")
    print("  PCs, directions, order, and metadata: all agree")
    print("  Note: validated only the prefix through the last converted branch.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Original standard ChampSim .xz")
    parser.add_argument("converted", type=Path, help="Converted branch-only text trace")
    args = parser.parse_args()
    try:
        verify(args.source, args.converted)
    except (VerificationError, OSError, EOFError, lzma.LZMAError, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
