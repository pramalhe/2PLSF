/*
 * Copyright 2022
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <cstdint>
#include <cstring>
#include "common/ThreadRegistry.hpp"

struct LatencyResult {
    uint64_t delay50000;
    uint64_t delay90000;
    uint64_t delay99000;
    uint64_t delay99900;
    uint64_t delay99990;
    uint64_t delay99999;
};


// This class stores latency (time) of each operation and aggregates it across multiples threads
class LatencyHistogram {
    // We store at most 10M raw time values per thread
    static const uint64_t MAX_RAW_VALUES { 100000000ULL };
    static const uint64_t CLPAD = 128/sizeof(uint64_t);
    static const int maxThreads = 32;

    uint64_t* rawDelays[maxThreads];
    uint64_t  lastEntry[maxThreads*CLPAD];

public:
    LatencyHistogram() {
        for (int it = 0; it < maxThreads; it++) {
            rawDelays[it] = new uint64_t[MAX_RAW_VALUES];
            lastEntry[it*CLPAD] = 0;
        }
    }

    ~LatencyHistogram() {
        for (int it = 0; it < maxThreads; it++) {
            delete[] rawDelays[it];
        }
    }

    // Pass to this auto startBeats = steady_clock::now()  .count()
    inline void addTimeMeasurement(uint64_t value, int tid) {
        assert(tid < maxThreads);
        rawDelays[tid][lastEntry[tid*CLPAD]] = value;
        lastEntry[tid*CLPAD]++;
    }

    // Aggregate the measurements from multiple threads
    void aggregateAll(void) {
        uint64_t totalMeasures = 0;
        for (int it = 0; it < maxThreads; it++) {
            totalMeasures += lastEntry[it*CLPAD];
        }
        std::vector<uint64_t> aggDelay(totalMeasures);
        uint64_t idx = 0;
        for (int it = 0; it < maxThreads; it++) {
            for (int i = 0; i < lastEntry[it*CLPAD]; i++) {
                aggDelay[idx] = rawDelays[it][i];
                idx++;
            }
        }
        // Sort the aggregated delays
        std::cout << "Sorting delays...\n";
        std::sort(aggDelay.begin(), aggDelay.end());
        // Compute the Percentile Pxxxx
        // Show the 50% (median), 90%, 99%, 99.9%, 99.99%, 99.999% and maximum in microsecond/nanoseconds units
        long per50000 = (long)(totalMeasures*50000LL/100000LL);
        long per70000 = (long)(totalMeasures*70000LL/100000LL);
        long per80000 = (long)(totalMeasures*80000LL/100000LL);
        long per90000 = (long)(totalMeasures*90000LL/100000LL);
        long per99000 = (long)(totalMeasures*99000LL/100000LL);
        long per99900 = (long)(totalMeasures*99900LL/100000LL);
        long per99990 = (long)(totalMeasures*99990LL/100000LL);
        long per99999 = (long)(totalMeasures*99999LL/100000LL);
        long imax = totalMeasures-1;

        std::cout << "Txn delay (us): 50%=" << aggDelay[per50000]/1000 << "  70%=" << aggDelay[per70000]/1000 << "  80%=" << aggDelay[per80000]/1000
             << "  90%=" << aggDelay[per90000]/1000 << "  99%=" << aggDelay[per99000]/1000
             << "  99.9%=" << aggDelay[per99900]/1000 << "  99.99%=" << aggDelay[per99990]/1000
             << "  99.999%=" << aggDelay[per99999]/1000 << "  max=" << aggDelay[imax]/1000 << "\n";

/*
        Result res = {
            (uint64_t)aggDelay[per50000].count()/1000, (uint64_t)aggDelay[per90000].count()/1000,
            (uint64_t)aggDelay[per99000].count()/1000, (uint64_t)aggDelay[per99900].count()/1000,
            (uint64_t)aggDelay[per99990].count()/1000, (uint64_t)aggDelay[per99999].count()/1000
        };
*/
    }


};
