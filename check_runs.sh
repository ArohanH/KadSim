#!/bin/bash
cd "$(dirname "$0")"
for d in blockchain-simulator-output_2026-04-08-14-18-02 blockchain-simulator-output_2026-04-08-14-18-06 blockchain-simulator-output_2026-04-08-14-18-11 blockchain-simulator-output_2026-04-08-14-18-13 blockchain-simulator-output_2026-04-08-14-18-17; do
    echo "=== $d ==="
    head -6 "$d/log"
    grep 'p50\|contention_fraction\|total_messages' "$d/stats.py" 2>/dev/null
    echo
done
