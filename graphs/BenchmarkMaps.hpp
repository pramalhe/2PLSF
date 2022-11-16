/*
 * Copyright 2017-2022
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
    template<typename T> struct tmtype {
        T val;
        tmtype() { }
        tmtype(T initVal) { pstore(initVal); }
        T pload() const { return val;}
        void pstore(T newVal) { val = newVal; }
    };
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
 * This is a micro-benchmark of maps
 */
class BenchmarkMaps {

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
    BenchmarkMaps(int numThreads) {
        this->numThreads = numThreads;
    }

    /**
     * When doing "writes" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    template<typename M, typename TM = DummySTM>
    long long benchmark(std::string& className, const int insertRatio, const int removeRatio, const int updateRatio, const int rqRatio,
                        const seconds testLengthSeconds, const int numRuns,
                        const uint64_t numKeys, const bool doDedicated=false, const uint64_t rqSize=false) {

        long long ops[numThreads+2][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        M* map = nullptr;

        // Make sure the ratios don't sum up to more than 100%. Whatever is below 100% the remaining is contains ratio
        assert (insertRatio + removeRatio + updateRatio + rqRatio <= 1000);

        // When running in dedicated mode, there are two extra threads (writers)
        if (doDedicated) numThreads += 2;

        className = M::className();
        std::cout << "##### " << M::className() << " #####  \n";

        // It's ok to capture by reference because we're running single-threaded
        TM::template updateTx<uint64_t>([&] () {
            map = TM::template tmNew<M>();
            return 0;
        });
        // Create all the keys in the concurrent map
        uint64_t* karray = new uint64_t[numKeys];
        for (int i = 0; i < numKeys; i++) karray[i] = i;

        // Create the values (records) in the STM memory pool
        Record** varray = nullptr;
        TM::template updateTx<uint64_t>([&] () {
            varray = (Record**)TM::tmMalloc(numKeys*sizeof(Record*));
            return 0;
        });
        // Create all the values in the concurrent map
        for (int i = 0; i < numKeys; i++) {
            TM::template updateTx<uint64_t>([&] () {
                varray[i] = TM::template tmNew<Record>();
                return 0;
            });
        }
        // Set a seed for the shuffle
        std::random_device rd;
        auto shuffle_seed = rd();
        std::mt19937 gen(shuffle_seed);
        // Shuffle the insertion order of the keys
        std::shuffle(karray, karray + numKeys, gen);
        // Add the first half of the key/values in the key space to the map
        map->addAll(karray, varray, numKeys/2);

        // Can either be a Reader or a Writer
        auto rw_lambda = [this,&quit,&startFlag,&map,&karray,&varray,&numKeys,&rqSize](
                const int insertRatio, const int removeRatio, const int updateRatio, const int rqRatio, long long *ops, const int tid) {
            long long numOps = 0;
            while (!startFlag.load()) ; // spin
            uint64_t seed = (tid+1)+12345678901234567ULL;
            uint64_t resultKeys[5000];
            while (!quit.load()) {
                seed = randomLong(seed);
                int optype = seed%1000;
                seed = randomLong(seed);
                uint64_t ix = seed % numKeys;
                if (optype < insertRatio) {
                    map->add(karray[ix], varray[ix]);
                } else if (optype < insertRatio+removeRatio) {
                    map->remove(karray[ix]);
                } else if (optype < insertRatio+removeRatio+updateRatio) {
                    // Find a key/value
                    Record* rec = nullptr;
                    if (rec = map->get(karray[ix])) {
                        // Chose a random field in the record to update
                        //seed = randomLong(seed);
                        //int fieldIndex = seed%RECORD_SIZE;
                        // Write in all fields of the record
                        TM::template updateTx<uint64_t>([&] () {
                            for (int i = 0; i < RECORD_SIZE; i++) {
                                rec->data[i].pstore(seed);
                            }
                            return 0;
                        });
                    }
                } else if (optype < insertRatio+removeRatio+updateRatio+rqRatio) {
                    // Range Query with rqSize keys maximum
                    //uint64_t numKeysInRangeQuery = map->rangeQuery(karray[ix], (karray[ix])+rqSize, resultKeys);
                    //printf("numKeysInRangeQuery=%ld from %ld to %ld\n", numKeysInRangeQuery, *udarray[ix], *udarray[ix]+100);
                } else { // contains() ratio
                    map->contains(karray[ix]);
                }
                numOps++;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            thread rwThreads[numThreads];
            if (doDedicated) {
                rwThreads[0] = thread(rw_lambda, 500, 500, 0, 0, &ops[0][irun], 0);
                rwThreads[1] = thread(rw_lambda, 500, 500, 0, 0, &ops[1][irun], 1);
                for (int tid = 2; tid < numThreads; tid++) {
                    rwThreads[tid] = thread(rw_lambda, insertRatio, removeRatio, updateRatio, rqRatio, &ops[tid][irun], tid);
                }
            } else {
                for (int tid = 0; tid < numThreads; tid++) {
                    rwThreads[tid] = thread(rw_lambda, insertRatio, removeRatio, updateRatio, rqRatio, &ops[tid][irun], tid);
                }
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

        // Clear the map, one key at a time and then delete the instance
        uint64_t numRemovedKeys = 0;
        for (int i = 0; i < numKeys; i++) if (map->remove(karray[i])) numRemovedKeys++;
        //printf("numRemovedKeys = %ld\n", numRemovedKeys);
        TM::template updateTx<uint64_t>([&] () {
            // TODO: get/put_object
            TM::tmDelete(map);
            return 0;
        });

        delete[] karray;
        for (int i = 0; i < numKeys; i++) {
            TM::template updateTx<uint64_t>([&] () {
                TM::template tmDelete(varray[i]);
                return 0;
            });
        }
        TM::template updateTx<uint64_t>([&] () {
            TM::tmFree(varray);
            return 0;
        });


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

