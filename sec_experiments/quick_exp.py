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
#  PAPER-READY PLOTTING FUNCTION
###############################################################################
def custom_paper_plot(data, filename, series_name, x_name, y_name, **kwargs):
    
    sys_threads = os.cpu_count()
    if sys_threads is None: 
        sys_threads = 56 # Fallback
    # --- 1. CLEAN THE FILENAME ---
    match = re.search(r"-i_(\d+)_-d_(\d+)", filename)
    
    if match:
        ins_val = match.group(1)
        del_val = match.group(2)
        directory = os.path.dirname(filename)
        clean_name = f"throughput_{sys_threads}_{ins_val}_{del_val}.png"
        filename = os.path.join(directory, clean_name)
        print(f"   [Renamed] Saving to: {clean_name}")
    else:
        print(f"Generating Plot: {filename}")

    # --- 2. DETECT SYSTEM THREADS ---
    oversub_val = os.cpu_count()
    if oversub_val is None: oversub_val = 56
        
    # --- 3. DATA PREPARATION ---
    cols = [series_name, x_name, y_name]
    df = pd.DataFrame(data, columns=cols)
    
    df[y_name] = pd.to_numeric(df[y_name], errors='coerce')
    df[x_name] = pd.to_numeric(df[x_name], errors='coerce')
    
    # Aggregate: Calculate MEAN only
    df_agg = df.groupby([series_name, x_name])[y_name].agg(['mean']).reset_index()

    # --- 4. STYLE CONFIGURATION ---
    plt.rcParams.update({
        'font.size': 14, 'font.family': 'serif',
        'axes.labelsize': 16, 'axes.titlesize': 16,
        'legend.fontsize': 12, 'pdf.fonttype': 42, 
    })

    fig, ax = plt.subplots(figsize=(8, 5.5))
    algs = df_agg[series_name].unique()
    colors = plt.cm.tab10.colors
    markers = ['D', 'o', 's', 'x', '>', '*']
    
    # --- 5. PLOTTING LOOP ---
    for i, alg in enumerate(algs):
        color = colors[i % len(colors)]
        marker = markers[i % len(markers)]
        
        is_highlight = "SEC" in alg
        lw = 3 if is_highlight else 2
        ms = 8 if is_highlight else 6
        
        sub_df = df_agg[df_agg[series_name] == alg].sort_values(by=x_name)
        
        ax.plot(
            sub_df[x_name], sub_df['mean'],
            label=alg, marker=marker, color=color,
            linewidth=lw, markersize=ms
        )

    # --- 6. AXIS DECORATION ---
    ax.set_xlabel("Threads")
    ax.set_ylabel("Throughput (ops/sec)")
    ax.grid(True, linestyle='--', alpha=0.6)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    # --- OVERSUBSCRIPTION LINE ---
    ax.axvline(x=oversub_val, linestyle="--", color="gray", alpha=0.7)

    # Custom X-Ticks
    max_threads = df_agg[x_name].max()
    xticks = list(range(0, int(max_threads) + 1, 24))
    if oversub_val not in xticks and oversub_val <= max_threads:
        xticks.append(oversub_val)
    
    unique_x = sorted(df_agg[x_name].unique().tolist())
    if len(unique_x) < 8: xticks = unique_x 
    if oversub_val not in xticks and oversub_val <= max_threads: xticks.append(oversub_val)
        
    xticks = sorted(list(set(xticks)))
    if 0 in xticks and len(xticks) > 1: xticks.remove(0)
    ax.set_xticks(xticks)

    ax.legend(loc='upper left', frameon=True, framealpha=0.9)
    plt.tight_layout()

    # --- 7. SAVE FILES (PNG ONLY) ---
    plt.savefig(filename, bbox_inches='tight')
    plt.close(fig)

###############################################################################
# HELPER TO READ INPUT FILES
###############################################################################
def read_config_file(filename, default_val):
    path = os.path.join(os.getcwd(), 'inputs', filename)
    if not os.path.exists(path):
        print(f"⚠️ Warning: {filename} not found. Using default: {default_val}")
        return default_val
    
    with open(path, 'r') as f:
        content = f.read().strip()
        if not content: return default_val
        # Handle comma or newline separated values
        items = [x.strip() for x in content.replace('\n', ',').split(',') if x.strip()]
        return items

###############################################################################
# DEFINE EXPERIMENT
###############################################################################
def define_experiment(exp_dict, args):

    # Directories
    set_dir_tools    (exp_dict, os.getcwd() + '/../tools')
    set_dir_compile  (exp_dict, os.getcwd() + '/../microbench')
    set_dir_run      (exp_dict, os.getcwd() + '/../microbench/bin')
    set_cmd_compile  (exp_dict, 'make -j16')
    
    # Store data in 'quick_exp' to match the shell script
    set_dir_data     (exp_dict, os.getcwd() + '/quick_exp')

    ###########################################################################
    # PARAMETERS (Read from inputs/ folder)
    ###########################################################################
    
    # 1. Algorithms
    add_run_param(exp_dict, 'DS_ALGOS', [
            'stacks_SEC', 
            # 'stacks_TSI', 
            # 'stacks_TRB',
            # 'stacks_CC',
            # 'stacks_FC',
            # 'stacks_EBF'
    ])

    # 2. Reclaimers (from inputs/reclaimer.txt)
    reclaimers = read_config_file('reclaimer.txt', ['debra'])
    add_run_param(exp_dict, 'RECLAIMER_ALGOS', reclaimers)

    # 3. Trials (from inputs/steps.txt)
    steps_str = read_config_file('steps.txt', ['1'])
    steps_int = [int(x) for x in steps_str]
    add_run_param(exp_dict, '__trials', steps_int)

    # 4. Thread Sequence (from inputs/threadsequence.txt)
    threads_str = read_config_file('threadsequence.txt', ['1', '14'])
    threads_int = [int(x) for x in threads_str]
    add_run_param(exp_dict, 'TOTAL_THREADS', threads_int)

    # 5. Workloads (from inputs/workloadtype.txt)
    raw_workloads = read_config_file('workloadtype.txt', ['50'])
    workload_configs = []
    
    for w in raw_workloads:
        w = str(w)
        if w == "50":
            workload_configs.append("-i 50 -d 50 -t 1000")
        elif w == "25":
            workload_configs.append("-i 25 -d 25 -t 1000")
        elif w == "5":
            workload_configs.append("-i 5 -d 5 -t 1000")
        elif w == "100":
            workload_configs.append("-i 100 -d 0 -t 1000")
        elif w == "0":
            workload_configs.append("-i 0 -d 100 -t 500") # Short Pop
        else:
            workload_configs.append(f"-i {w} -d {w} -t 1000")

    add_run_param(exp_dict, 'WORKLOAD_CONFIG', workload_configs)

    # --- FIX: Pass as a LIST ---
    keys_int = [5000] 
    add_run_param(exp_dict, 'DS_SIZE', keys_int)

    ###########################################################################
    # COMMAND TEMPLATE
    ###########################################################################
    set_cmd_run(
        exp_dict,
        'LD_PRELOAD=../../lib/libmimalloc.so time '
        './{DS_ALGOS}.{RECLAIMER_ALGOS} '
        '-nwork {TOTAL_THREADS} '
        '-nprefill {TOTAL_THREADS} '
        '{WORKLOAD_CONFIG} ' 
        '-rq 0 -rqsize 1 '
        '-k {DS_SIZE} '
    )

    ###########################################################################
    # PLOTTING
    ###########################################################################
    add_data_field(exp_dict, 'total_throughput', coltype='INTEGER')

    add_plot_set(
        exp_dict,
        name='throughput_{WORKLOAD_CONFIG}.png',
        varying_cols_list=['WORKLOAD_CONFIG', 'DS_SIZE'], 
        series='DS_ALGOS',
        title='Throughput (Config: {WORKLOAD_CONFIG})',
        x_axis='TOTAL_THREADS',
        y_axis='total_throughput',
        plot_type=custom_paper_plot, 
        plot_cmd_args={'x_label': 'Threads', 'y_label': 'Throughput'}
    )