import re
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
exe = root / ('branch_sim.exe' if sys.platform == 'win32' else 'branch_sim')
if not exe.exists():
    raise SystemExit('Compile branch_sim.c first.')

def run(*args):
    return subprocess.run([str(exe), *args], cwd=root, text=True, capture_output=True, check=True).stdout

built = run()
custom = run('traces/my_trace.txt')
assert 'branches=580' in built, built
assert 'branches=280' in custom, custom
assert re.search(r'Always-Taken\s+390/580\s+67\.24%', built), built
assert re.search(r'GShare \(h=12,size=1024\)\s+474/580\s+81\.72%', built), built
assert 'MPKI=N/A' in built, built
print('PASS: built-in baseline and custom trace checks')
