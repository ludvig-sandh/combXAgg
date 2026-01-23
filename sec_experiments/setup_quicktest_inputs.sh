#!/bin/bash

# Create inputs directory if it doesn't exist
mkdir -p inputs

echo "Creating default input files in inputs/..."

# The reclaimer used in the paper
echo "debra" > inputs/reclaimer.txt

# Just 1 step/trial for quick testing
echo "1" > inputs/steps.txt

# A small sequence for quick verification
echo "1,14,28,40,56" > inputs/threadsequence.txt

# Default to balanced workload (50% Insert / 50% Delete)
# You can add others here like: 50,100,0
echo "50" > inputs/workloadtype.txt

echo "✅ Inputs setup complete. You can now edit files in the 'inputs/' folder."