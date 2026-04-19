#!/bin/bash
# 800-node experiment sweep: baseline vs adaptive vs dynamic vs both
# All use α=0.5, β=3, dense, seeds 49/41
cd "$(dirname "$0")"

COMMON="800 0.5 0.5 10 10000 600 40000 dense 10 0.5"
SEEDS="49 41"

echo "============================================================"
echo "  800-node sweep: 4 runs × (Classic + KADcast)"
echo "  Started: $(date)"
echo "============================================================"

echo ""
echo "[1/4] Baseline (no adaptive, no dynamic)"
python3 run_experiment.py $COMMON $SEEDS

echo ""
echo "[2/4] Adaptive β only"
python3 run_experiment.py $COMMON adaptive $SEEDS

echo ""
echo "[3/4] Dynamic β only"
python3 run_experiment.py $COMMON dynamic $SEEDS

echo ""
echo "[4/4] Adaptive + Dynamic β"
python3 run_experiment.py $COMMON adaptive dynamic $SEEDS

echo ""
echo "============================================================"
echo "  All 4 runs complete: $(date)"
echo "============================================================"
echo ""
echo "Results files:"
ls -1t results_*.txt | head -4
