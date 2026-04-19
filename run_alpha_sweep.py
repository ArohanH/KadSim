#!/usr/bin/env python3
"""
Alpha-sweep experiment: runs 9 experiments varying alpha from 0.0 to 1.0
with all other parameters fixed.

Parameters (fixed):
  800 nodes, z0=0.5, z1=0.5, cpu_ratio=10,
  txn_iat=10000, block_iat=600, duration=40000,
  dense graph, beta=3, no adaptive, no dynamic,
  seeds: graph=49 sim=41
"""

import subprocess
import sys

ALPHA_VALUES = [0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0]

BASE_ARGS = [
    "800",    # nr_nodes
    "0.5",    # z_0
    "0.5",    # z_1
    "10",     # cpu_ratio
    "10000",  # txn_iat
    "600",    # block_iat
    "40000",  # duration
    "dense",  # density
    "10",      # beta
]

SEEDS = ["41", "49"]

def main():
    total = len(ALPHA_VALUES)
    for idx, alpha in enumerate(ALPHA_VALUES, 1):
        print(f"\n{'=' * 60}")
        print(f"  Run {idx}/{total}: alpha={alpha}")
        print(f"{'=' * 60}")
        cmd = [sys.executable, "run_experiment.py"] + BASE_ARGS + [str(alpha)] + SEEDS
        print(f"  Command: {' '.join(cmd)}")
        result = subprocess.run(cmd)
        if result.returncode != 0:
            print(f"  WARNING: run with alpha={alpha} exited with code {result.returncode}")

    print(f"\n{'=' * 60}")
    print(f"  All {total} alpha-sweep runs complete!")
    print(f"{'=' * 60}")

if __name__ == "__main__":
    main()
