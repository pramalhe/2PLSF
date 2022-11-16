#!/usr/bin/env python
import os
import time

#thread_list = [1,4,8,10,12,14,16]             # List of threads to execute. This one is for Legion laptop
thread_list = [1,4,8,16,32,48,56,64]    # List of threads for cervino-2
workload = "YCSB"

print "\n\n+++ Running DBx1000 microbenchmarks +++\n"

if workload == "YCSB":
    print "Make sure you built this with #define WORKLOAD YCSB and ABORT_BUFFER_ENABLE false in config.h"
    # "High contention", from page 1637 of https://dl.acm.org/doi/pdf/10.1145/2882903.2882935 
    os.system("rm ycsb-high-results.txt")
    for thread in thread_list:
        os.system("./rundb -o output.txt -t"+ str(thread) + " -r0.5 -w0.5 -R16 -z0.9")
        os.system("cat output.txt >> ycsb-high-results.txt")
        time.sleep(1)

    # "Medium contention"
    os.system("rm ycsb-med-results.txt")
    for thread in thread_list:
        os.system("./rundb -o output.txt -t"+ str(thread) + " -r0.9 -w0.1 -R16 -z0.8")
        os.system("cat output.txt >> ycsb-med-results.txt")
        time.sleep(1)

    # "Low contention"
    os.system("rm ycsb-low-results.txt")
    for thread in thread_list:
        os.system("./rundb -o output.txt -t"+ str(thread) + " -r1.0 -w0 -R2 -z0")
        os.system("cat output.txt >> ycsb-low-results.txt")
        time.sleep(1)



if workload == "TPCC":
    print "Make sure you built this with #define WORKLOAD TPCC in config.h\n"
    # "High contention", from page 1637 of https://dl.acm.org/doi/pdf/10.1145/2882903.2882935 
    os.system("rm tpcc-results.txt")
    for thread in thread_list:
        os.system("./rundb -o output.txt -t"+ str(thread) + " -n4")
        os.system("cat output.txt >> tpcc-results.txt")
        time.sleep(1)
    
    
    
