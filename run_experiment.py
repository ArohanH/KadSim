#!/usr/bin/env python3
"""
KadSim Experiment Runner
Runs both KADcast and Classic (flooding) simulations with the same parameters,
then runs analyze.py and saves everything to a results text file.

Usage:
    python run_experiment.py <nr_nodes> <z_0> <z_1> <cpu_ratio> <txn_iat> <block_iat> <duration> [sparse|moderate|dense] [beta] [graph_seed sim_seed]

Example:
    python run_experiment.py 200 0.5 0.5 10 5000 60000 600000 dense 3 42 42
    python run_experiment.py 100 0.5 0.5 10 5000 60000 600000 moderate 1
    python run_experiment.py 100 0.5 0.5 10 5000 60000 600000 sparse

If seeds are not provided, random seeds are used (but the SAME seeds for both runs).
"""

import os
import subprocess
import sys
import random
import datetime
import glob


def find_latest_output_dir(before_dirs):
    """Find the newest output directory that wasn't in before_dirs."""
    all_dirs = set(glob.glob("blockchain-simulator-output_*"))
    new_dirs = all_dirs - before_dirs
    if not new_dirs:
        return None
    return max(new_dirs, key=os.path.getmtime)


def run_simulation(binary, nr_nodes, z_0, z_1, cpu_ratio, txn_iat, block_iat,
                   duration, protocol, density, beta, graph_seed, sim_seed):
    """Run a single simulation and return the output directory path."""
    before_dirs = set(glob.glob("blockchain-simulator-output_*"))

    cmd = [
        binary,
        str(nr_nodes), str(z_0), str(z_1), str(cpu_ratio),
        str(txn_iat), str(block_iat), str(duration),
        protocol, density, str(beta), str(graph_seed), str(sim_seed),
    ]

    print(f"  Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"  ERROR: Simulation failed!")
        print(f"  stdout: {result.stdout}")
        print(f"  stderr: {result.stderr}")
        return None

    output_dir = find_latest_output_dir(before_dirs)
    if output_dir:
        print(f"  Output: {output_dir}")
    else:
        print(f"  WARNING: Could not find output directory")
    return output_dir


def run_analysis(classic_dir, kadcast_dir):
    """Run analyze.py and capture the output."""
    cmd = [sys.executable, "analyze.py", classic_dir, kadcast_dir]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout


def main():
    if len(sys.argv) < 8:
        print(__doc__)
        sys.exit(1)

    nr_nodes = sys.argv[1]
    z_0 = sys.argv[2]
    z_1 = sys.argv[3]
    cpu_ratio = sys.argv[4]
    txn_iat = sys.argv[5]
    block_iat = sys.argv[6]
    duration = sys.argv[7]

    density = "dense"
    beta = 3
    next_arg = 8
    if len(sys.argv) > next_arg and sys.argv[next_arg] in ("sparse", "moderate", "dense"):
        density = sys.argv[next_arg]
        next_arg += 1

    if len(sys.argv) > next_arg:
        try:
            val = int(sys.argv[next_arg])
            if 1 <= val <= 10:
                beta = val
                next_arg += 1
        except ValueError:
            pass

    if len(sys.argv) > next_arg + 1:
        graph_seed = int(sys.argv[next_arg])
        sim_seed = int(sys.argv[next_arg + 1])
    else:
        graph_seed = random.randint(0, 2**31)
        sim_seed = random.randint(0, 2**31)

    # Find the simulator binary
    binary = os.path.join("bin", "kadsim")
    if not os.path.exists(binary):
        # Try WSL-style path
        binary = "./bin/kadsim"
    if not os.path.exists(binary):
        print("Error: Cannot find bin/kadsim. Build the project first (make -j).")
        sys.exit(1)

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    results_file = f"results_{density}_{nr_nodes}nodes_b{beta}_{timestamp}.txt"

    print(f"=" * 60)
    print(f"  KadSim Experiment: {nr_nodes} nodes, {density} graph, beta={beta}")
    print(f"  Seeds: graph={graph_seed}, sim={sim_seed}")
    print(f"=" * 60)

    # Run Classic (flooding)
    print(f"\n[1/2] Running Classic (flooding) simulation...")
    classic_dir = run_simulation(
        binary, nr_nodes, z_0, z_1, cpu_ratio, txn_iat, block_iat,
        duration, "Classic", density, beta, graph_seed, sim_seed)

    if not classic_dir:
        print("Classic simulation failed. Aborting.")
        sys.exit(1)

    # Run KADcast
    print(f"\n[2/2] Running KADcast simulation...")
    kadcast_dir = run_simulation(
        binary, nr_nodes, z_0, z_1, cpu_ratio, txn_iat, block_iat,
        duration, "Kadcast", density, beta, graph_seed, sim_seed)

    if not kadcast_dir:
        print("KADcast simulation failed. Aborting.")
        sys.exit(1)

    # Run analysis
    print(f"\n[3/3] Running analysis...")
    analysis_output = run_analysis(classic_dir, kadcast_dir)
    print(analysis_output)

    # Write results to file
    with open(results_file, "w") as f:
        f.write(f"KadSim Experiment Results\n")
        f.write(f"Generated: {timestamp}\n")
        f.write(f"Parameters: nr_nodes={nr_nodes} z_0={z_0} z_1={z_1} cpu_ratio={cpu_ratio}\n")
        f.write(f"            txn_iat={txn_iat} block_iat={block_iat} duration={duration}\n")
        f.write(f"            density={density} beta={beta} graph_seed={graph_seed} sim_seed={sim_seed}\n")
        f.write(f"Classic output:  {classic_dir}\n")
        f.write(f"KADcast output:  {kadcast_dir}\n")
        f.write(f"\n")
        f.write(analysis_output)

        # Also append raw stats
        f.write(f"\n{'=' * 60}\n")
        f.write(f"  RAW STATS: Classic ({classic_dir})\n")
        f.write(f"{'=' * 60}\n")
        stats_path = os.path.join(classic_dir, "stats.py")
        if os.path.exists(stats_path):
            with open(stats_path) as sf:
                f.write(sf.read())

        f.write(f"\n{'=' * 60}\n")
        f.write(f"  RAW STATS: KADcast ({kadcast_dir})\n")
        f.write(f"{'=' * 60}\n")
        stats_path = os.path.join(kadcast_dir, "stats.py")
        if os.path.exists(stats_path):
            with open(stats_path) as sf:
                f.write(sf.read())

    print(f"\nResults saved to: {results_file}")


if __name__ == "__main__":
    main()
