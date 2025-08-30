#!/bin/bash

# Runtime in milliseconds
runtime_ms=5000

# Insert/Delete percentage pairs
configs=(
  "5 5"
  "25 25"
  "50 50"
  "100 0"
)

# Algorithms to test
algorithms=(
  stacks_flat_combining
  stacks_bsf_1
  stacks_bsf_2
  stacks_bsf_3
  stacks_bsf_4
  stacks_bsf_5
  stacks_ouropt_bsf_1
  stacks_ouropt_bsf_2
  stacks_ouropt_bsf_3
  stacks_ouropt_bsf_4
  stacks_ouropt_bsf_5
  stacks_ebf
  stacks_treiber_backoff
  stacks_ts_interval_hardware
  stacks_cc_elim
)

# Thread counts
threads=(1 12 24 36 48 60 72 84 96 108 120 132 144 156 168 180 192 204 216 228 240 252)

# Number of trials per configuration
trials=5

# Build once before running tests
echo "🔨 Building binaries..."
(
    cd .. && make DATA_STRUCTURES="${algorithms[*]}" -j16
)

for config in "${configs[@]}"; do
    ip=$(echo $config | cut -d' ' -f1)
    dp=$(echo $config | cut -d' ' -f2)

    echo ""
    echo "🚀 Starting config: ${ip}% insert / ${dp}% delete"
    echo "================================================"

    output_file="all_paper_data_${ip}_${dp}.csv"
    rm -f "$output_file"

    cols="%35s %12s %12s\n"
    printf "$cols" alg nthreads tput >> "$output_file"

    for n in "${threads[@]}"; do
        echo "➡️  Threads: $n"
        for alg in "${algorithms[@]}"; do
            echo "   ▶ Running $alg with $n threads ($trials trials)"
            for ((trial=0; trial<trials; ++trial)); do
                echo "      Trial $((trial+1))/$trials..."

                sfname="step_titan_${alg}_${ip}_${dp}_${n}_${trial}.txt"
                if [ "$n" -gt 96 ]; then
                    LD_PRELOAD=../../lib/libmimalloc.so \
                    time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                    numactl --interleave=all \
                    ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                    -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms -prefillsize 1000 \
                    > "$sfname"
                else
                    LD_PRELOAD=../../lib/libmimalloc.so \
                    time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                    numactl --interleave=all \
                    ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                    -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms \
                    -pin 0-11,48-59,12-23,60-71,24-35,72-83,36-47,84-95 -prefillsize 1000 \
                    > "$sfname"
                fi
                tput=$(grep "total throughput" "$sfname" | cut -d ":" -f2 | tr -d " ")
                max_resmem=$(grep memory timetemp.txt | tail -1 | cut -d":" -f2)

                printf "%35s %12d %12d\n" "$alg" "$n" "$tput" >> "$output_file"
            done
        done
        echo "✅ Finished all algorithms for $n threads"
        echo "-------------------"
    done

    echo "🎯 Finished full config: ${ip}% insert / ${dp}% delete"
    echo "Results saved to $output_file"
    echo "================================================"
done
echo "🏁 All experiments completed successfully!"