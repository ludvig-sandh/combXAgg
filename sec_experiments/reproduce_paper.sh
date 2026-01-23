#!/bin/bash

# --- CONFIGURATION ---
THREADS=$(nproc)
unified_script="experiment.py"
final_plot_base="plots/generated_plots"
framework_cmd="python3 ../tools/data_framework/run_experiment.py"

# Create base directory for plots
mkdir -p $final_plot_base

echo "========================================================"
echo "      SEC Artifact Reproduction Script"
echo "      Detected System Threads: $THREADS"
echo "========================================================"

# ---------------------------------------------------------
# Step 1: COMPARISON EXPERIMENTS
# (Python script handles 56 vs 96 thread logic internally)
# ---------------------------------------------------------
echo "Step 1: Running Comparison Experiments..."
export EXP_MODE="comparison"
data_dir="comparison_experiments"

echo "   [Comparison] Compiling..."
$framework_cmd $unified_script -c

echo "   [Comparison] Running & Plotting..."
# Note: On 56 threads, this automatically includes the Pop-Only workload (-i 0)
# with the correct prefill flags.
$framework_cmd $unified_script -rdp

echo "   [Comparison] Gathering Plots..."
if [ -d "$data_dir" ]; then
    cp $data_dir/*.png $final_plot_base/ 2>/dev/null || true
else
    echo "   Warning: $data_dir not found."
fi


# ---------------------------------------------------------
# Step 2: AGGREGATION EXPERIMENTS (Conditional)
# ---------------------------------------------------------
if [ "$THREADS" -eq 96 ]; then
    echo " "
    echo "--------------------------------------------------------"
    echo "Skipping Aggregation Experiments (Configured for 96-thread limit)."
    echo "Total plots generated: 3"
    echo "--------------------------------------------------------"
else
    echo " "
    echo "--------------------------------------------------------"
    echo "Step 2: Running Aggregation Experiments..."
    echo "--------------------------------------------------------"
    
    export EXP_MODE="aggs"
    data_dir="exp_aggs"

    echo "   [Aggs] Compiling..."
    $framework_cmd $unified_script -c

    echo "   [Aggs] Running & Plotting..."
    $framework_cmd $unified_script -rdp

    echo "   [Aggs] Gathering Plots..."
    if [ -d "$data_dir" ]; then
        cp $data_dir/*.png $final_plot_base/ 2>/dev/null || true
    else
        echo "   Warning: $data_dir not found."
    fi

    echo "Total plots generated: 9 (5 Comparison + 4 Aggregation)"
fi

echo " "
echo "========================================================"
echo "Done! Check $final_plot_base"
echo "========================================================"