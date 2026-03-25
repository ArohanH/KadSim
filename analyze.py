#!/usr/bin/env python3
"""
KadSim Analysis Script
Compare KADcast vs Classic (flooding) simulation results.

Usage:
    python analyze.py <classic_output_dir> <kadcast_output_dir>
    python analyze.py <single_output_dir>
"""

import sys
import os
import math


def load_stats(directory):
    """Load stats.py from a simulation output directory."""
    stats_path = os.path.join(directory, "stats.py")
    if not os.path.exists(stats_path):
        print(f"Error: {stats_path} not found")
        sys.exit(1)
    ns = {}
    with open(stats_path, "r") as f:
        exec(f.read(), ns)
    return ns


def percentile(data, p):
    """Compute the p-th percentile of a list."""
    if not data:
        return 0.0
    s = sorted(data)
    k = (len(s) - 1) * p / 100.0
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return s[int(k)]
    return s[f] * (c - k) + s[c] * (k - f)


def analyze_propagation_delays(block_propagation_delays, nr_nodes):
    """Analyze per-block propagation delays and return summary dict."""
    if not block_propagation_delays:
        return {}

    all_p50, all_p90, all_p99, all_max, all_mean = [], [], [], [], []
    coverage_fractions = []  # fraction of nodes that received each block

    for block_id, delays in block_propagation_delays.items():
        if not delays:
            continue
        s = sorted(delays)
        n = len(s)
        all_p50.append(percentile(delays, 50))
        all_p90.append(percentile(delays, 90))
        all_p99.append(percentile(delays, 99))
        all_max.append(max(delays))
        all_mean.append(sum(delays) / n)
        # +1 because the miner itself has delay=0 (excluded from delays list)
        coverage_fractions.append((n + 1) / nr_nodes if nr_nodes > 0 else 0)

    return {
        "num_blocks_analyzed": len(all_p50),
        "median_p50_ms": percentile(all_p50, 50),
        "median_p90_ms": percentile(all_p90, 50),
        "median_p99_ms": percentile(all_p99, 50),
        "median_max_ms": percentile(all_max, 50),
        "median_mean_ms": percentile(all_mean, 50),
        "mean_p50_ms": sum(all_p50) / len(all_p50) if all_p50 else 0,
        "mean_p90_ms": sum(all_p90) / len(all_p90) if all_p90 else 0,
        "mean_coverage": sum(coverage_fractions) / len(coverage_fractions) if coverage_fractions else 0,
    }


def print_single(stats, label=""):
    """Print analysis for a single simulation run."""
    if label:
        print(f"\n{'='*60}")
        print(f"  {label}")
        print(f"{'='*60}")

    protocol = stats.get("overlay_protocol", "unknown")
    density = stats.get("graph_density", "unknown")
    nr_nodes = stats.get("nr_nodes", 0)
    duration = stats.get("duration", 0)

    print(f"\n  Protocol:          {protocol}")
    print(f"  Graph density:     {density}")
    print(f"  Nodes:             {nr_nodes}")
    print(f"  Duration:          {duration:.0f} ms")
    print(f"")

    total = stats.get("total_blocks", 0)
    longest = stats.get("longest_chain_length", 0)
    stale = stats.get("stale_blocks", 0)
    stale_rate = stats.get("stale_rate", 0)
    print(f"  --- Consensus ---")
    print(f"  Total blocks mined:     {total}")
    print(f"  Longest chain length:   {longest}")
    print(f"  Stale blocks:           {stale}")
    print(f"  Stale rate:             {stale_rate*100:.2f}%")
    print(f"")

    msgs = stats.get("total_messages_sent", 0)
    traffic = stats.get("total_traffic_kb", 0)
    print(f"  --- Network Load ---")
    print(f"  Total messages sent:    {msgs}")
    print(f"  Total traffic:          {traffic:.1f} KB ({traffic/1024:.1f} MB)")
    if nr_nodes > 0:
        print(f"  Messages per node:      {msgs/nr_nodes:.1f}")
        print(f"  Traffic per node:       {traffic/nr_nodes:.1f} KB")
    print(f"")

    bp_delays = stats.get("block_propagation_delays", {})
    if bp_delays:
        analysis = analyze_propagation_delays(bp_delays, nr_nodes)
        print(f"  --- Block Propagation Delay (ms) ---")
        print(f"  Blocks analyzed:        {analysis['num_blocks_analyzed']}")
        print(f"  Median of per-block p50:  {analysis['median_p50_ms']:.1f}")
        print(f"  Median of per-block p90:  {analysis['median_p90_ms']:.1f}")
        print(f"  Median of per-block p99:  {analysis['median_p99_ms']:.1f}")
        print(f"  Median of per-block max:  {analysis['median_max_ms']:.1f}")
        print(f"  Mean block coverage:      {analysis['mean_coverage']*100:.1f}%")
    else:
        print(f"  No propagation delay data available.")

    print(f"")
    print(f"  --- Mining Distribution ---")
    fh = stats.get("blocks_fast_highcpu", 0)
    fl = stats.get("blocks_fast_lowcpu", 0)
    sh = stats.get("blocks_slow_highcpu", 0)
    sl = stats.get("blocks_slow_lowcpu", 0)
    print(f"  Fast+HighCPU: {fh}  Fast+LowCPU: {fl}  Slow+HighCPU: {sh}  Slow+LowCPU: {sl}")

    fh = stats.get("blocks_in_longest_chain_fast_highcpu", 0)
    fl = stats.get("blocks_in_longest_chain_fast_lowcpu", 0)
    sh = stats.get("blocks_in_longest_chain_slow_highcpu", 0)
    sl = stats.get("blocks_in_longest_chain_slow_lowcpu", 0)
    print(f"  In longest chain:")
    print(f"  Fast+HighCPU: {fh}  Fast+LowCPU: {fl}  Slow+HighCPU: {sh}  Slow+LowCPU: {sl}")


def print_comparison(classic, kadcast):
    """Print side-by-side comparison."""
    print(f"\n{'='*60}")
    print(f"  COMPARISON: Classic vs KADcast")
    print(f"{'='*60}")

    cn = classic.get("nr_nodes", 0)
    kn = kadcast.get("nr_nodes", 0)
    print(f"\n  Nodes: Classic={cn}  KADcast={kn}")

    c_total = classic.get("total_blocks", 0)
    k_total = kadcast.get("total_blocks", 0)
    c_stale = classic.get("stale_rate", 0)
    k_stale = kadcast.get("stale_rate", 0)
    print(f"\n  --- Consensus ---")
    print(f"  {'Metric':<30} {'Classic':>12} {'KADcast':>12} {'Improvement':>12}")
    print(f"  {'-'*66}")
    print(f"  {'Total blocks':<30} {c_total:>12} {k_total:>12}")
    print(f"  {'Stale rate':<30} {c_stale*100:>11.2f}% {k_stale*100:>11.2f}%")

    c_msgs = classic.get("total_messages_sent", 0)
    k_msgs = kadcast.get("total_messages_sent", 0)
    c_traffic = classic.get("total_traffic_kb", 0)
    k_traffic = kadcast.get("total_traffic_kb", 0)
    msg_reduction = (1 - k_msgs / c_msgs) * 100 if c_msgs > 0 else 0
    traffic_reduction = (1 - k_traffic / c_traffic) * 100 if c_traffic > 0 else 0
    print(f"\n  --- Network Load ---")
    print(f"  {'Metric':<30} {'Classic':>12} {'KADcast':>12} {'Reduction':>12}")
    print(f"  {'-'*66}")
    print(f"  {'Total messages':<30} {c_msgs:>12} {k_msgs:>12} {msg_reduction:>11.1f}%")
    print(f"  {'Total traffic (KB)':<30} {c_traffic:>12.0f} {k_traffic:>12.0f} {traffic_reduction:>11.1f}%")

    c_bp = classic.get("block_propagation_delays", {})
    k_bp = kadcast.get("block_propagation_delays", {})
    if c_bp and k_bp:
        c_a = analyze_propagation_delays(c_bp, cn)
        k_a = analyze_propagation_delays(k_bp, kn)
        p90_improvement = (1 - k_a["median_p90_ms"] / c_a["median_p90_ms"]) * 100 if c_a["median_p90_ms"] > 0 else 0
        p50_improvement = (1 - k_a["median_p50_ms"] / c_a["median_p50_ms"]) * 100 if c_a["median_p50_ms"] > 0 else 0

        print(f"\n  --- Block Propagation Delay (ms) ---")
        print(f"  {'Metric':<30} {'Classic':>12} {'KADcast':>12} {'Faster by':>12}")
        print(f"  {'-'*66}")
        print(f"  {'Median p50':<30} {c_a['median_p50_ms']:>12.1f} {k_a['median_p50_ms']:>12.1f} {p50_improvement:>11.1f}%")
        print(f"  {'Median p90':<30} {c_a['median_p90_ms']:>12.1f} {k_a['median_p90_ms']:>12.1f} {p90_improvement:>11.1f}%")
        print(f"  {'Median p99':<30} {c_a['median_p99_ms']:>12.1f} {k_a['median_p99_ms']:>12.1f}")
        print(f"  {'Mean coverage':<30} {c_a['mean_coverage']*100:>11.1f}% {k_a['mean_coverage']*100:>11.1f}%")

    print()


def main():
    if len(sys.argv) == 2:
        stats = load_stats(sys.argv[1])
        print_single(stats, label=sys.argv[1])
    elif len(sys.argv) == 3:
        classic = load_stats(sys.argv[1])
        kadcast = load_stats(sys.argv[2])
        print_single(classic, label=f"Classic: {sys.argv[1]}")
        print_single(kadcast, label=f"KADcast: {sys.argv[2]}")
        print_comparison(classic, kadcast)
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
