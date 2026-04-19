#!/bin/bash
# 800-node experiment sweep for MODERATE and SPARSE densities
# All use α=0.5, β=3, seeds 49/41
cd "$(dirname "$0")"

COMMON_BASE="800 0.5 0.5 100 10000 600 40000"
SEEDS="49 41"

echo "============================================================"
echo "  800-node sweep: moderate + sparse × 4 configs"
echo "  Started: $(date)"
echo "============================================================"

for DENSITY in moderate sparse; do
    echo ""
    echo "########################################################"
    echo "  DENSITY: $DENSITY"
    echo "########################################################"

    COMMON="$COMMON_BASE $DENSITY 10 0.5"

    echo ""
    echo "[$DENSITY 1/4] Baseline (no adaptive, no dynamic)"
    python3 run_experiment.py $COMMON $SEEDS

    echo ""
    echo "[$DENSITY 2/4] Adaptive β only"
    python3 run_experiment.py $COMMON adaptive $SEEDS

    echo ""
    echo "[$DENSITY 3/4] Dynamic β only"
    python3 run_experiment.py $COMMON dynamic $SEEDS

    echo ""
    echo "[$DENSITY 4/4] Adaptive + Dynamic β"
    python3 run_experiment.py $COMMON adaptive dynamic $SEEDS
done

echo ""
echo "============================================================"
echo "  All runs complete: $(date)"
echo "============================================================"
echo ""
echo "New results files:"
ls -1t results_*.txt | head -8
