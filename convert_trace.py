import gzip
import lzma
import struct
import sys

RECORD_SIZE = 64


def convert_trace(input_path, output_path, target_branches=600_000):
  total_instructions = 0
  total_branches = 0
  branch_records = []

  opener = gzip.open if input_path.lower().endswith(".gz") else lzma.open

  with opener(input_path, "rb") as fin:
    while True:
      chunk = fin.read(RECORD_SIZE)
      if not chunk:
        break

      if len(chunk) != RECORD_SIZE:
        raise ValueError("Incomplete ChampSim record")

      ip, is_branch, branch_taken = struct.unpack("<QBB", chunk[:10])

      if is_branch not in (0, 1) or branch_taken not in (0, 1):
        raise ValueError("Invalid ChampSim record format")

      total_instructions += 1

      if is_branch:
        total_branches += 1
        outcome = "T" if branch_taken else "N"
        branch_records.append(f"0x{ip:x} {outcome}\n")

        if total_branches >= target_branches:
          break

  with open(output_path, "w") as fout:
    fout.write(f"# Source: {input_path}\n")
    fout.write(f"# Branches: {total_branches}\n")
    fout.write(f"# Verified instructions: {total_instructions}\n")
    fout.writelines(branch_records)

  print(f"Saved: {output_path}")
  print(f"  Branches: {total_branches}")
  print(f"  Instructions: {total_instructions}")


if __name__ == "__main__":
  if len(sys.argv) != 3:
    print("Usage: python convert_trace.py <input.xz/.gz> <output.txt>")
    sys.exit(1)

  convert_trace(sys.argv[1], sys.argv[2])