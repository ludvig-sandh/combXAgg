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
#   MASTER CONFIGURATION
###############################################################################

EXPERIMENT_CONFIG = {
    # 1. OUTPUT SETTINGS
    # ------------------
    "output_dir": "./plots/generated_plots/custom_run", # <--- PLOTS GO HERE

    # 2. EXPERIMENT PARAMETERS
    # ------------------------
    "algorithms": [
        'stacks_SEC', 'stacks_TSI', 'stacks_FC', 
        'stacks_EBF', 'stacks_TRB', 'stacks_CC'
    ],
    "reclaimers": ['debra'],
    "trials": [1, 2, 3],
    "workloads": [
        {"i": 5, "d": 5},
        {"i": 25, "d": 25},
        {"i": 50, "d": 50},
        {"i": 100, "d": 0},
    ],
    
    # 3. ENVIRONMENT
    # --------------
    "thread_sequence": [1, 14, 28, 42, 56, 70, 84, 96], 
    "binary_path": "./{DS_ALGOS}.{RECLAIMER_ALGOS}",
    "preload_lib": "../../lib/libmimalloc.so",
    "duration_ms": 5000,
    "key_range": 5000,
}

###############################################################################
#   HELPER: THREAD VALIDATION
###############################################################################
def get_thread_list():
    threads = EXPERIMENT_CONFIG.get("thread_sequence")
    if not threads:
        sys.exit(0)
    return threads

###############################################################################
#   PLOTTING FUNCTION
###############################################################################
def dynamic_plotter(data, filename, series_name, x_name, y_name, **kwargs):
    
    # 1. SETUP OUTPUT DIRECTORY
    #    We ignore the 'filename' path passed by the framework (which points to /data)
    #    and use our custom output directory instead.
    target_dir = os.path.join(os.getcwd(), EXPERIMENT_CONFIG['output_dir'])
    if not os.path.exists(target_dir):
        os.makedirs(target_dir, exist_ok=True)

    # 2. GENERATE CLEAN FILENAME
    #    Extract params from the "ugly" framework filename (e.g. throughput_-i 50 -d 50.png)
    base_name = os.path.basename(filename)
    match = re.search(r"-i\s*(\d+).*-d\s*(\d+)", base_name)
    
    if match:
        actual_name = f"throughput_i{match.group(1)}_d{match.group(2)}.png"
    else:
        clean_base = base_name.replace(" ", "_").replace("-", "").replace(".png", "")
        actual_name = f"{clean_base}.png"

    full_path = os.path.join(target_dir, actual_name)

    # 3. PREPARE DATA
    cols = [series_name, x_name, y_name]
    df = pd.DataFrame(data, columns=cols)
    df[y_name] = pd.to_numeric(df[y_name], errors='coerce')
    df[x_name] = pd.to_numeric(df[x_name], errors='coerce')
    
    df_agg = df.groupby([series_name, x_name])[y_name].mean().reset_index()

    # 4. PLOT
    plt.rcParams.update({'font.size': 14, 'font.family': 'serif', 'pdf.fonttype': 42})
    fig, ax = plt.subplots(figsize=(8, 5.5))
    
    algs = df_agg[series_name].unique()
    colors = plt.cm.tab10.colors
    markers = ['D', 'o', 's', 'x', '>', '*', 'v', '^']
    
    for i, alg in enumerate(algs):
        is_highlight = "SEC" in alg
        lw = 3 if is_highlight else 2
        
        sub_df = df_agg[df_agg[series_name] == alg].sort_values(by=x_name)
        
        ax.plot(
            sub_df[x_name], sub_df[y_name],
            label=alg, 
            marker=markers[i % len(markers)], 
            color=colors[i % len(colors)],
            linewidth=lw, 
            markersize=8
        )

    ax.set_xlabel("Threads")
    ax.set_ylabel("Throughput (ops/sec)")
    ax.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    
    unique_x = sorted(df_agg[x_name].unique())
    if len(unique_x) > 16:
        ax.set_xticks(unique_x[::2])
    else:
        ax.set_xticks(unique_x)

    ax.legend(loc='upper left', frameon=True, framealpha=0.9)
    plt.tight_layout()

    print(f"   [Plot] Saved to: {full_path}")
    plt.savefig(full_path, bbox_inches='tight')
    plt.close(fig)

###############################################################################
#   EXPERIMENT DEFINITION
###############################################################################
def define_experiment(exp_dict, args=None):
    
    # 1. SETUP
    thread_list = get_thread_list()
    wl_strings = [
        f"-i {w['i']} -d {w['d']}" 
        for w in EXPERIMENT_CONFIG['workloads']
    ]

    add_run_param(exp_dict, 'DS_ALGOS', EXPERIMENT_CONFIG['algorithms'])
    add_run_param(exp_dict, 'RECLAIMER_ALGOS', EXPERIMENT_CONFIG['reclaimers'])
    add_run_param(exp_dict, '__trials', EXPERIMENT_CONFIG['trials'])
    add_run_param(exp_dict, 'TOTAL_THREADS', thread_list)
    add_run_param(exp_dict, 'WORKLOAD_CONFIG', wl_strings)

    # 2. PATHS
    set_dir_run(exp_dict, '../microbench/bin')
    set_dir_data(exp_dict, os.getcwd() + '/data')

    # 3. COMMAND
    cmd = (
        f"LD_PRELOAD={EXPERIMENT_CONFIG['preload_lib']} "
        "time "
        f"{EXPERIMENT_CONFIG['binary_path']} "
        "-nwork {TOTAL_THREADS} "
        "-nprefill {TOTAL_THREADS} "
        f"-t {EXPERIMENT_CONFIG['duration_ms']} "
        f"-k {EXPERIMENT_CONFIG['key_range']} "
        "{WORKLOAD_CONFIG} "
        "-rq 0 -rqsize 1"
    )
    
    set_cmd_run(exp_dict, cmd)

    # 4. PLOTTING
    add_data_field(exp_dict, 'total_throughput', coltype='INTEGER')

    # We use {WORKLOAD_CONFIG} in the name to ensure unique plot tasks.
    # The plotter logic redirects the file save to EXPERIMENT_CONFIG['output_dir']
    add_plot_set(
        exp_dict,
        name='throughput_{WORKLOAD_CONFIG}.png', 
        varying_cols_list=['WORKLOAD_CONFIG'], 
        series='DS_ALGOS',
        title='Throughput Analysis',
        x_axis='TOTAL_THREADS',
        y_axis='total_throughput',
        plot_type=dynamic_plotter
    )