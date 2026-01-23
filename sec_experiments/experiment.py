import sys 
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import re

# Add tools directory to path
sys.path.append('../tools/data_framework')

# Import the framework
from run_experiment import *
from _basic_functions import *

###############################################################################
#  SHARED HELPER: THREAD LIST GENERATION
###############################################################################
def get_thread_configuration():
    """Returns (sys_threads, thread_list) based on hardware detection."""
    sys_threads = os.cpu_count()
    if sys_threads is None: 
        sys_threads = 56
    
    # print(f"Detected System Threads: {sys_threads}")

    if sys_threads == 56:
        t_list = [1, 14, 28, 42, 56, 70, 84, 98]
    elif sys_threads == 96:
        t_list = [1, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120]
    else:
        t_list = sorted(list(set([1, sys_threads // 2, sys_threads, int(sys_threads * 1.25)])))
    
    return sys_threads, t_list

###############################################################################
#  SHARED HELPER: FILENAME RESOLUTION
###############################################################################
def resolve_filename(original_filename, mode, sys_threads):
    """Determines the output filename based on mode and workload parameters."""
    dirname = os.path.dirname(original_filename)
    basename = os.path.basename(original_filename)
    
    # Robust regex to find insert/delete percentages
    match = re.search(r"-i[\s_]+(\d+).*?-d[\s_]+(\d+)", basename)
    
    if not match:
        print(f"   [Warning] Could not parse workload params from: {basename}")
        return original_filename 
        
    ins = int(match.group(1))
    dele = int(match.group(2))
    
    new_name = None

    # --- MODE 1: AGGS (Figure 4 Mapping) ---
    if mode == 'aggs':
        mapping = {
            (50, 50): "fig4_left_most.png",
            (25, 25): "fig4_mid_left.png",
            (5, 5):   "fig4_mid_right.png",
            (100, 0): "fig4_right_most.png"
        }
        if (ins, dele) in mapping:
            new_name = mapping[(ins, dele)]
        else:
            new_name = f"throughput_agg_{sys_threads}_{ins}_{dele}.png"

    # --- MODE 2: COMPARISON (Figure 2/3 Mapping) ---
    elif mode == 'comparison':
        mapping = {
            # Figure 2a (56 Threads)
            (56, 50, 50): "fig2_a_left.png",
            (56, 25, 25): "fig2_a_mid.png",
            (56, 5, 5):   "fig2_a_right.png",
            # Figure 2b (96 Threads)
            (96, 50, 50): "fig2_b_left.png",
            (96, 25, 25): "fig2_b_mid.png",
            (96, 5, 5):   "fig2_b_right.png",
            # Figure 3 (Comparison Extremes)
            (56, 100, 0): "fig3_left.png",
            (56, 0, 100): "fig3_right.png" 
        }
        
        # We try to match specific threads first
        key = (sys_threads, ins, dele)
        
        if key in mapping:
            new_name = mapping[key]
        else:
            # Fallback for non-mapped workloads
            new_name = f"throughput_{sys_threads}_{ins}_{dele}.png"

    if new_name:
        print(f"   [{mode.upper()} Mapped] Saving plot as: {new_name}")
        return os.path.join(dirname, new_name)
    
    return original_filename

###############################################################################
#  UNIFIED PLOTTING FUNCTION
###############################################################################
def unified_paper_plot(data, filename, series_name, x_name, y_name, **kwargs):
    
    cols = [series_name, x_name, y_name]
    df = pd.DataFrame(data, columns=cols)
    df[y_name] = pd.to_numeric(df[y_name], errors='coerce')
    df[x_name] = pd.to_numeric(df[x_name], errors='coerce')
    
    # --- AUTO-DETECT MODE ---
    # Fix: Sometimes kwargs get dropped in parallel execution. 
    # We inspect the algorithm names to determine the mode.
    unique_algos = df[series_name].unique().tolist()
    is_agg_experiment = any("agg" in str(alg) for alg in unique_algos)
    
    if is_agg_experiment:
        exp_mode = 'aggs'
    else:
        exp_mode = 'comparison'

    sys_threads, _ = get_thread_configuration()
    
    # 1. Resolve the correct filename (Figure 4 vs Figure 2)
    filename = resolve_filename(filename, exp_mode, sys_threads)
    
    # Aggregation for plotting
    df_agg = df.groupby([series_name, x_name])[y_name].agg(['mean']).reset_index()

    plt.rcParams.update({
        'font.size': 14, 'font.family': 'serif',
        'axes.labelsize': 16, 'axes.titlesize': 16,
        'legend.fontsize': 12, 'pdf.fonttype': 42
    })

    fig, ax = plt.subplots(figsize=(8, 5.5))
    algs = df_agg[series_name].unique()
    colors = plt.cm.tab10.colors
    markers = ['D', 'o', 's', 'x', '>', '*']
    
    for i, alg in enumerate(algs):
        color = colors[i % len(colors)]
        marker = markers[i % len(markers)]
        
        is_highlight = "SEC" in alg
        lw = 3 if is_highlight else 2
        ms = 8 if is_highlight else 6
        
        # Clean label (e.g., stacks_SEC_agg1 -> SEC_Agg1)
        clean_label = alg.replace("stacks_", "").replace("agg", "Agg")

        sub_df = df_agg[df_agg[series_name] == alg].sort_values(by=x_name)
        
        ax.plot(
            sub_df[x_name], sub_df['mean'],
            label=clean_label, 
            marker=marker, color=color,
            linewidth=lw, markersize=ms
        )

    ax.set_xlabel("Threads")
    ax.set_ylabel("Throughput (ops/sec)")
    ax.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.axvline(x=sys_threads, linestyle="--", color="gray", alpha=0.7)

    max_threads = df_agg[x_name].max()
    unique_x = sorted(df_agg[x_name].unique().tolist())
    
    step_size = 24 
    
    if len(unique_x) <= 15:
        xticks = unique_x
    else:
        xticks = list(range(0, int(max_threads) + 1, step_size))

    if sys_threads not in xticks and sys_threads <= max_threads:
        xticks.append(sys_threads)
        
    xticks = sorted(list(set(xticks)))
    if 0 in xticks and len(xticks) > 1: xticks.remove(0)
    ax.set_xticks(xticks)

    ax.legend(loc='upper left', frameon=True, framealpha=0.9)
    plt.tight_layout()

    plt.savefig(filename, bbox_inches='tight')
    plt.close(fig)

###############################################################################
#  MAIN EXPERIMENT DEFINITION
###############################################################################
def define_experiment(exp_dict, args):

    mode = "comparison" 
    if "--mode" in sys.argv:
        try:
            mode = sys.argv[sys.argv.index("--mode") + 1]
        except: pass
    elif os.environ.get("EXP_MODE"):
        mode = os.environ.get("EXP_MODE")
    
    mode = mode.lower().strip()
    if mode == "poponly": mode = "comparison" 
    
    print(f">>> Configuring Experiment Mode: {mode.upper()}")

    # --- 2. COMMON DIRECTORIES ---
    set_dir_tools(exp_dict, os.getcwd() + '/../tools')
    set_dir_compile(exp_dict, os.getcwd() + '/../microbench')
    set_dir_run(exp_dict, os.getcwd() + '/../microbench/bin')
    set_cmd_compile(exp_dict, 'make -j16')

    # Data directory
    dir_map = {'aggs': 'exp_aggs', 'comparison': 'comparison_experiments'}
    set_dir_data(exp_dict, os.getcwd() + f"/{dir_map.get(mode, 'comparison_experiments')}")

    # --- 3. THREADS ---
    sys_threads, thread_list = get_thread_configuration()
    print(f"Running with Thread List: {thread_list}")

    # --- 4. CONFIGURATION SWITCH ---
    ds_algos = []
    workloads = []
    
    if mode == 'aggs':
        ds_algos = ["stacks_SEC_agg1", "stacks_SEC_agg2", "stacks_SEC_agg3", "stacks_SEC_agg4", "stacks_SEC_agg5"]
        workloads = [
            "-i 50 -d 50 -t 5000 -k 5000 -prefillsize 1000",
            "-i 25 -d 25 -t 5000 -k 5000 -prefillsize 1000",
            "-i 5 -d 5 -t 5000 -k 5000 -prefillsize 1000",
            "-i 100 -d 0 -t 5000 -k 5000 -prefillsize 1000",
        ]

    else: # Default: COMPARISON
        ds_algos = ['stacks_SEC', 'stacks_TSI', 'stacks_FC', 'stacks_EBF', 'stacks_TRB', 'stacks_CC']
        
        if sys_threads == 56:
             workloads = [
                "-i 50 -d 50 -t 5000 -k 5000 -prefillsize 1000",
                "-i 25 -d 25 -t 5000 -k 5000 -prefillsize 1000",
                "-i 5 -d 5 -t 5000 -k 5000 -prefillsize 1000",
                "-i 100 -d 0 -t 5000 -k 5000 -prefillsize 1000",
                "-i 0 -d 100 -t 2000 -k 5000 -prefillsize 1000" 
            ]
        else: 
             workloads = [
                "-i 50 -d 50 -t 5000 -k 5000 -prefillsize 1000",
                "-i 25 -d 25 -t 5000 -k 5000 -prefillsize 1000",
                "-i 5 -d 5 -t 5000 -k 5000 -prefillsize 1000"
            ]

    # --- 5. REGISTER PARAMETERS ---
    add_run_param(exp_dict, 'DS_ALGOS', ds_algos)
    add_run_param(exp_dict, 'RECLAIMER_ALGOS', ['debra'])
    add_run_param(exp_dict, '__trials', [1,2,3])
    add_run_param(exp_dict, 'TOTAL_THREADS', thread_list)
    add_run_param(exp_dict, 'WORKLOAD_CONFIG', workloads)

    extra_cmd = r'''$(if echo "{WORKLOAD_CONFIG}" | grep -q -- "-i 0 "; then
    case "{DS_ALGOS}" in
        stacks_FC|*FC*)  echo "-prefillsize 100000000" ;;
        stacks_EBF|*EBF*) echo "-prefillsize 1000000" ;;
        stacks_TRB|*TRB*) echo "-prefillsize 80000000" ;;
        stacks_TSI|*TSI*) echo "-prefillsize 80000000" ;;
        stacks_CC|*CC*)   echo "-prefillsize 80000000" ;;
        *)                echo "-prefillsize 15000000" ;;
    esac;
    echo "{DS_ALGOS}" | grep -q "FC" || echo "-prefill-insert";
    fi) '''

    set_cmd_run(
        exp_dict,
        'LD_PRELOAD=../../lib/libmimalloc.so time '
        './{DS_ALGOS}.{RECLAIMER_ALGOS} '
        '-nwork {TOTAL_THREADS} '
        '-nprefill {TOTAL_THREADS} '
        '{WORKLOAD_CONFIG} ' 
        f'{extra_cmd}'
        '-rq 0 -rqsize 1 '
    )

    # --- 7. PLOTTING ---
    add_data_field(exp_dict, 'total_throughput', coltype='INTEGER')

    add_plot_set(
        exp_dict,
        name='throughput_{WORKLOAD_CONFIG}.png',
        varying_cols_list=['WORKLOAD_CONFIG'], 
        series='DS_ALGOS',
        title=f'Throughput ({mode.capitalize()})',
        x_axis='TOTAL_THREADS',
        y_axis='total_throughput',
        plot_type=unified_paper_plot, 
        plot_cmd_args={'x_label': 'Threads', 'y_label': 'Throughput', 'exp_mode': mode}
    )