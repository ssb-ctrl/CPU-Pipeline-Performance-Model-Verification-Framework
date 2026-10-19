#!/usr/bin/env python3
"""Verification flow: generate random programs, run them on the pipeline in all 6 configs,
compare against the reference model. Usage: python3 fuzz.py [N_PROGRAMS] [--inject-bug]"""
import subprocess, sys
from gen_random import gen

n = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 300
extra = ["--inject-bug"] if "--inject-bug" in sys.argv else []
fails = 0
for seed in range(n):
    with open("/tmp/fuzz_prog.asm", "w") as f:
        f.write(gen(seed, 30 + seed % 40))
    rc = subprocess.run(["./pipeline_sim", "/tmp/fuzz_prog.asm", "--sweep", "--quiet"] + extra).returncode
    if rc != 0:
        fails += 1
        if fails == 1:
            open("failing_program.asm", "w").write(gen(seed, 30 + seed % 40))
            print(f"first failure: seed {seed} (saved as failing_program.asm)")
print(f"{n - fails}/{n} random programs passed" + ("  <-- BUG DETECTED" if fails else ""))
sys.exit(1 if fails else 0)
