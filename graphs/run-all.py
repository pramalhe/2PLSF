#!/usr/bin/env python2
import os

bin_folder = "bin/"
time_duration = "20"                                # Duration of each run in seconds
num_runs = "1"                                      # Number of runs. The final result will tbe median of all runs
#thread_list = "1,2,3,4,6,8,10,12,14,16"             # List of threads to execute. This one is for Legion laptop
thread_list = "1,4,8,12,16,24,32,40,48,56,64"    # List of threads for cervino-2
#thread_list = "1,4,8,12,16,24,32,40,48,55"       # List of threads for cervino-2 with 2plundo
#thread_list = "1,4,8,12,16,20,24,32,40"          # List of threads to execute. This one is for castor-1/pollux
#thread_list = "1,4,8,16,32,48,64"                # List of threads to execute. This one is for moleson-1
#thread_list = "1,4,8,16,32,48,64,80,96"          # List of threads to execute. This one is for AWS c5.24xlarge or c5d.24xlarge
#thread_list = "1,4,8,16,32,64,96,128,160,192"    # List of threads to execute. This one is for AWS c6a.48xlarge with 192 vCPUs
ratio_list = "1000,200,0"                        # Write ratios in permils (100 means 10% writes and 90% reads)
#ratio_list = "100"                            # For the original TL2, use 0% writes
cmd_line_options = " --duration="+time_duration+"  --runs="+num_runs+" --threads="+thread_list+" --ratios="+ratio_list

# STMs that we want to have in our benchmarks
stm_name_list = [
    "2plsf",          # Two-Phase locking with distributed RW-Lock and Starvation-Free, made by us
    "tiny",           # Tiny STM
    "tl2orig",        # Original TL2
    "tlrweager",      # TLRW from https://github.com/mfs409/llvm-transmem
    "oreceager",      # Orec with eager locking, by Zardoshti et al, from https://github.com/mfs409/llvm-transmem 
    "ofwf",           # OneFile Wait-Free by Ramalhete et al, from https://github.com/pramalhe/OneFile/
    "tl2",            # TL2 from https://github.com/mfs409/llvm-transmem
    "oreclazy",
    "2plundo",        # Two-Phase locking with RW-Lock. Only supports 56 threads (55 in our benchmark)
    "2plundodist",    # Two-Phase locking with distributed RW-Lock
]



print "\n\n+++ Running concurrent microbenchmarks +++\n"

#for stm in stm_name_list:
#    os.system(bin_folder+"set-ll-1k-"+ stm + cmd_line_options + " --keys=1000")

for stm in stm_name_list:
    os.system(bin_folder+"set-ravl-1m-"+ stm + cmd_line_options + " --keys=1000000")

for stm in stm_name_list:
    os.system(bin_folder+"set-skiplist-1m-"+ stm + cmd_line_options + " --keys=1000000")

for stm in stm_name_list:
    os.system(bin_folder+"set-ziptree-1m-"+ stm + cmd_line_options + " --keys=1000000")

#for stm in stm_name_list:
#    os.system(bin_folder+"set-tree-1m-"+ stm + cmd_line_options + " --keys=1000000")

#for stm in stm_name_list:
#    os.system(bin_folder+"set-btree-1m-"+ stm + cmd_line_options + " --keys=1000000")

for stm in stm_name_list:
    os.system(bin_folder+"set-hash-10k-"+ stm + cmd_line_options + " --keys=10000")

# Benchmark with the latency measures
#os.system("rm latency.log")
#for stm in stm_name_list:
#    os.system(bin_folder+"part-disjoint-"+ stm + " --duration=20 --threads=1,4,8,16,24,32 >> latency.log")

# Benchmark with maps and updates on records (values)
for stm in stm_name_list:
    os.system(bin_folder+"map-ravl-"+ stm +     " --keys=100000 --duration="+time_duration+" --ratios=1000 --threads="+thread_list)
for stm in stm_name_list:
    os.system(bin_folder+"map-skiplist-"+ stm + " --keys=100000 --duration="+time_duration+" --ratios=1000 --threads="+thread_list)
for stm in stm_name_list:
    os.system(bin_folder+"map-ziptree-"+ stm +  " --keys=100000 --duration="+time_duration+" --ratios=1000 --threads="+thread_list)

