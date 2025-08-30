#!/bin/bash

# Runtime in milliseconds
runtime_ms=2000

# Insert/Delete percentage pairs
configs=(
  "0 100"
)

# Algorithms to test
algorithms=(
  stacks_treiber_backoff
)

# Thread counts
threads=(12 24)

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

    output_file="TRB_paper_data_titan_${ip}_${dp}.csv"
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
                if [ "$alg" = "stacks_flat_combining" ]; then
                    if [ "$n" -gt 96 ]; then
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms -prefillsize 15000000 \
                        > "$sfname"
                    else
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms \
                        -pin 0-11,48-59,12-23,60-71,24-35,72-83,36-47,84-95 -prefillsize 15000000 \
                        > "$sfname"
                    fi
                elif [ "$alg" = "stacks_cc_elim" ]; then # This is a special case for CCElim, on titan so that it doesn't empty the stack early for a fair throughput comparison with other algorithms
                    if [ "$n" -gt 96 ]; then
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms -prefillsize 60000000 \
                        > "$sfname"
                    else
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms \
                        -pin 0-11,48-59,12-23,60-71,24-35,72-83,36-47,84-95 -prefillsize 60000000 \
                        > "$sfname"
                    fi
                else
                    if [ "$n" -gt 96 ]; then
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms -prefillsize 80000000 -prefill-insert\
                        > "$sfname"
                    else
                        LD_PRELOAD=../../lib/libmimalloc.so \
                        time -f "cmd:%C \nmemory:%M" -o timetemp.txt \
                        numactl --interleave=all \
                        ./${alg}.debra -nwork $n -nprefill $n -i $ip -d $dp \
                        -rq 0 -rqsize 1 -k 1000 -nrq 0 -t $runtime_ms \
                        -pin 0-11,48-59,12-23,60-71,24-35,72-83,36-47,84-95 -prefillsize 80000000 -prefill-insert \
                        > "$sfname"
                    fi
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