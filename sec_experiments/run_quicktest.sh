#!/bin/sh
## This script compiles, runs, and then produces figures using Setbench's tool framework.

data_dir="quick_exp"
exp_file="quick_exp.py"

echo " "
echo "############################################"
echo "Compiling benchmark...(may take up to 2 minutes)"
echo "############################################"

# Compile with the clean flag
python3 ../tools/data_framework/run_experiment.py $exp_file -c

echo "############################################"
echo "Executing and generating FIGURES for EXP1..."
echo "############################################"

# Run, Dump Data, Plot (-rdp)
python3 ../tools/data_framework/run_experiment.py $exp_file -rdp

# Create output directory if it doesn't exist
mkdir -p plots/generated_plots/plot_$data_dir

echo "copying FIGURES to plots/generated_plots/plot_$data_dir/ "
# Move the PNGs from the data folder to the plot folder
cp $data_dir/*.png plots/generated_plots/plot_$data_dir/

echo "✅ Experiments Complete."