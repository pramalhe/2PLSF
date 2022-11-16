/*
 * Copyright 2017-2020
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
#include <iostream>
#include <cassert>
#include <random>

using namespace std;
using namespace chrono;

// Regular UserData
struct UserData  {
    long long seq;
    int tid;
    UserData(long long lseq, int ltid=0) {
        this->seq = lseq;
        this->tid = ltid;
    }
    UserData() {
        this->seq = -2;
        this->tid = -2;
    }
    UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

    bool operator < (const UserData& other) const {
        return seq < other.seq;
    }
    bool operator == (const UserData& other) const {
        return seq == other.seq && tid == other.tid;
    }
    bool operator != (const UserData& other) const {
        return seq != other.seq || tid != other.tid;
    }
};

// Just a mock STM so that we can do allocations and de-allocations inside a lambda.
// We use it to run the same benchmarks as the hand-made data structures.
struct DummySTM {
    template<typename R, typename F> static R updateTx(F&& func) { return func(); }
    template<typename T, typename... Args> static T* tmNew(Args&&... args) { return new T(args...); }
    template<typename T> static void tmDelete(T* obj) { delete obj; }
};


namespace std {
    template <>
    struct hash<UserData> {
        std::size_t operator()(const UserData& k) const {
            using std::size_t;
            using std::hash;
            return (hash<long long>()(k.seq));  // This hash has no collisions, which is irealistic
        }
    };
}


/**
 * This is a micro-benchmark of sets
 */
class BenchmarkSets {

private:
    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };

    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
    BenchmarkSets(int numThreads) {
        this->numThreads = numThreads;
    }

    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    template<typename S, typename K, typename TM = DummySTM>
    long long benchmark(std::string& className, const int updateRatio, const seconds testLengthSeconds, const int numRuns,
                        const uint64_t numElements, const bool doDedicated=false, const uint64_t rqSize=false) {
        long long ops[numThreads+2][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        S* set = nullptr;

        // When running in dedicated mode, there are two extra threads (writers)
        if (doDedicated) numThreads += 2;

        className = S::className();
        std::cout << "##### " << S::className() << " #####  \n";

        // It's ok to capture by reference because we're running single-threaded
        TM::template updateTx<uint64_t>([&] () {
            set = TM::template tmNew<S>();
            return 0;
        });
        // Create all the keys in the concurrent set
        K** udarray = new K*[numElements];
        for (int i = 0; i < numElements; i++) udarray[i] = new K(i);
        // Set a seed for the shuffle
        std::random_device rd;
        auto shuffle_seed = rd();
        std::mt19937 gen(shuffle_seed);
        // Shuffle the insertion order of the keys
        std::shuffle(udarray, udarray + numElements, gen);
        // Add all the items to the list
        set->addAll(udarray, numElements);

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&quit,&startFlag,&set,&udarray,&numElements,&rqSize](const int updateRatio, long long *ops, const int tid) {
            long long numOps = 0;
            while (!startFlag.load()) ; // spin
            uint64_t seed = (tid+1)+12345678901234567ULL;
            K resultKeys[5000];
            while (!quit.load()) {
                seed = randomLong(seed);
                int update = seed%1000;
                seed = randomLong(seed);
                uint64_t ix = seed % numElements;
                if (update < updateRatio) {
                    // I'm a Writer
                    if (set->remove(*udarray[ix])) {
                    	numOps++;
                    	set->add(*udarray[ix]);
                    }
                    numOps++;
                } else {
                    // I'm a Reader
                    if (rqSize != 0) {
                        // Range Query with rqSize keys maximum
                        uint64_t numKeysInRangeQuery = set->rangeQuery(*udarray[ix], (*udarray[ix])+rqSize, resultKeys);
                        //printf("numKeysInRangeQuery=%ld from %ld to %ld\n", numKeysInRangeQuery, *udarray[ix], *udarray[ix]+100);
                        numOps += 1;
                    } else {
                        // Short lookup
                        set->contains(*udarray[ix]);
                        seed = randomLong(seed);
                        ix = seed % numElements;
                        set->contains(*udarray[ix]);
                        numOps += 2;
                    }
                }
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            thread rwThreads[numThreads];
            if (doDedicated) {
                rwThreads[0] = thread(rw_lambda, 1000, &ops[0][irun], 0);
                rwThreads[1] = thread(rw_lambda, 1000, &ops[1][irun], 1);
                for (int tid = 2; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            } else {
                for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            }
            this_thread::sleep_for(100ms);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for testLengthSeconds seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            if (doDedicated) {
                // We don't account for the write-only operations but we aggregate the values from the two threads and display them
                std::cout << "Mutative transactions per second = " << (ops[0][irun] + ops[1][irun])*1000000000LL/lengthSec[irun] << "\n";
                ops[0][irun] = 0;
                ops[1][irun] = 0;
            }
            quit.store(false);
            startFlag.store(false);
            // Compute ops at the end of each run
            long long agg = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        // Clear the set, one key at a time and then delete the instance
        uint64_t numRemovedKeys = 0;
        for (int i = 0; i < numElements; i++) if (set->remove(*udarray[i])) numRemovedKeys++;
        //printf("numRemovedKeys = %ld\n", numRemovedKeys);
        TM::template updateTx<uint64_t>([&] () {
            // TODO: get/put_object
            TM::tmDelete(set);
            return 0;
        });

        for (int i = 0; i < numElements; i++) delete udarray[i];
        delete[] udarray;

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        // Compute the median. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "      delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
    }



    /*
     * Inspired by Trevor Brown's benchmarks (does everyone else do it like this?)
     */
    template<typename S, typename K>
    long long benchmarkRandomFill(std::string& className, const int updateRatio, const seconds testLengthSeconds, const int numRuns, const int numElements, const bool dedicated=false) {
        long long ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };

        className = S::className();
        std::cout << "##### " << S::className() << " #####  \n";
        S* set = new S(numThreads);
        // Create all the keys in the concurrent set
        K** udarray = new K*[2*numElements];
        for (int i = 0; i < 2*numElements; i++) udarray[i] = new K(i);
        // Add half the keys to the list
        long ielem = 0;
        uint64_t seed = 1234567890123456781ULL;
        while (ielem < numElements/2) {
            seed = randomLong(seed);
            // Insert new random keys until we have 'numElements/2' keys in the tree
            if (set->add(*udarray[seed%(numElements)], 0)) ielem++;
        }
        // Add all keys, repeating if needed
        set->addAll(udarray, numElements, 0);

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&quit,&startFlag,&set,&udarray,&numElements](const int updateRatio, long long *ops, const int tid) {
            long long numOps = 0;
            while (!startFlag.load()) ; // spin
            uint64_t seed = tid+1234567890123456781ULL;
            while (!quit.load()) {
                seed = randomLong(seed);
                int update = seed%1000;
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%numElements);
                if (update < updateRatio) {
                    // I'm a Writer
                    if (set->remove(*udarray[ix], tid)) {
                        numOps++;
                        set->add(*udarray[ix], tid);
                    }
                    numOps++;
                } else {
                    // I'm a Reader
                    set->contains(*udarray[ix], tid);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%numElements);
                    set->contains(*udarray[ix], tid);
                    numOps += 2;
                }
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            thread rwThreads[numThreads];
            if (dedicated) {
                rwThreads[0] = thread(rw_lambda, 1000, &ops[0][irun], 0);
                rwThreads[1] = thread(rw_lambda, 1000, &ops[1][irun], 1);
                for (int tid = 2; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            } else {
                for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            }
            this_thread::sleep_for(100ms);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for testLengthSeconds seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            if (dedicated) {
                // We don't account for the write-only operations but we aggregate the values from the two threads and display them
                std::cout << "Mutative transactions per second = " << (ops[0][irun] + ops[1][irun])*1000000000LL/lengthSec[irun] << "\n";
                ops[0][irun] = 0;
                ops[1][irun] = 0;
            }
            quit.store(false);
            startFlag.store(false);
            // Compute ops at the end of each run
            long long agg = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        /* Clear the tree, one key at a time and then delete the instance */
        for (int i = 0; i < numElements; i++) set->remove(*udarray[i], 0);
        delete set;

        for (int i = 0; i < numElements; i++) delete udarray[i];
        delete[] udarray;

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        // Compute the median. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "      delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        return medianops;
    }

    /**
     * An imprecise but fast random number generator
     */
    uint64_t randomLong(uint64_t x) {
        x ^= x >> 12; // a
        x ^= x << 25; // b
        x ^= x >> 27; // c
        return x * 2685821657736338717LL;
    }
};

