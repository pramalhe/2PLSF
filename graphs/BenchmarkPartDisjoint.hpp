/*
 * Copyright 2022
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <typeinfo>
#include "common/ThreadRegistry.hpp"
#include "common/LatencyHistogram.hpp"

using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark where we have partially disjoint workloads.
 * Threads have conflicts pair-wise.
 * We can also have all moving in the same direction or moving in opposite directions
 *
 */
class BenchmarkPartDisjoint {

private:
    int numThreads;
    // We create 20 counters per pair of threads, which means that, on average, a thread will
    // write to three counters before encountering a conflict, if the threads are in the opposite direction.
    // Make sure this is an even number.
    static const int COUNTERS_PER_THREAD_PAIR = 20;

    LatencyHistogram histo {};

public:
    BenchmarkPartDisjoint(int numThreads) {
        this->numThreads = numThreads;
    }


    template<typename TM, template<typename> class TMTYPE>
    uint64_t benchmark(std::string& className, const seconds testLengthSeconds, const int numRuns) {
        long long ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> startFlag = { false };
        atomic<bool> quit = { false };

        className = TM::className();
        cout << "##### " << TM::className() << " #####  \n";

        struct Counter {
            TMTYPE<uint64_t> count {0};
            uint8_t padding[512-8];
        };

        // Create the array of counters
        Counter* parray = nullptr;
        const uint64_t arraySize = numThreads*COUNTERS_PER_THREAD_PAIR;
        // It's ok to capture by reference, we're running single-threaded now
        TM::template updateTx<uint64_t>([&] () {
            parray = (Counter*)TM::tmMalloc(sizeof(Counter)*arraySize);
            return 0;
        } );

        auto func = [this,&startFlag,&quit,&parray](long long *ops, const int tid) {
            // Spin until the startFlag is set
            while (!startFlag.load()) {}
            // Do transactions until the quit flag is set
            long long tcount = 0;
            while (!quit.load()) {
                auto startBeats = steady_clock::now();
                TM::template updateTx<uint64_t>([=] () {
                    if (tid%2 == 0) {
                        // Go front to back
                        int idx = (tid/2)*COUNTERS_PER_THREAD_PAIR*2;
                        //printf("tid=%d [%d %d]\n", tid, idx, idx+COUNTERS_PER_THREAD_PAIR);
                        for (int i = 0; i < COUNTERS_PER_THREAD_PAIR; i++) {
                            parray[idx+i].count = parray[idx+i].count+1;
                            // Sleep a little to make the transaction slower
                            std::this_thread::sleep_for(1ns);
                        }
                    } else {
                        // Go back to front
                        int idx = ((tid/2)*COUNTERS_PER_THREAD_PAIR*2) + COUNTERS_PER_THREAD_PAIR-1;
                        //printf("tid=%d [%d %d]\n", tid, idx-COUNTERS_PER_THREAD_PAIR, idx);
                        for (int i = 0; i < COUNTERS_PER_THREAD_PAIR; i++) {
                            parray[idx-i].count = parray[idx-i].count+1;
                            // Sleep a little to make the transaction slower
                            std::this_thread::sleep_for(1ns);
                        }
                    }
                    return 0;
                } );
                ++tcount;
                auto stopBeats = steady_clock::now();
                histo.addTimeMeasurement((stopBeats-startBeats).count(), tid);
            }
            *ops = tcount;
        };
        for (int irun = 0; irun < numRuns; irun++) {
            if (irun == 0) className = TM::className();
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(func, &ops[tid][irun], tid);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for 20 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            startFlag.store(false);
            quit.store(false);
        }

        // It's ok to capture by reference, we're running single-threaded now
        TM::template updateTx<uint64_t>([&] () {
            TM::tmFree(parray);
            return 0;
        });

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
        	for(int i=0;i<numThreads;i++){
        		agg[irun] += ops[i][irun]*1000000000LL/lengthSec[irun];
        	}
        }
        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Txn/sec = " << medianops << "     delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        // Aggregate and show histogram
        histo.aggregateAll();
        return medianops;
    }
};

