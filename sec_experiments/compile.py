import sys
import os

# Add tools directory to path
sys.path.append('../tools/data_framework')

# Import the framework
from run_experiment import *
from _basic_functions import *

def define_experiment(exp_dict, args):
    """
    Compilation only.
    """
    set_dir_tools(exp_dict, os.getcwd() + '/../tools')
    set_dir_compile(exp_dict, os.getcwd() + '/../microbench')
    set_dir_run(exp_dict, os.getcwd() + '/../microbench/bin')

    set_cmd_compile(exp_dict, 'make -j16')


    add_run_param(exp_dict, 'dummy_param', ['1'])
    set_cmd_run(exp_dict, 'echo "Compilation complete"')