#include <iostream>
#include <fstream>
#include <cstring>

#include "common/CmdLineConfig.hpp"
#include "pdatastructures/TMBTree.hpp"
#include "pdatastructures/TMBTreeByRef.hpp"

// Macros suck, but we can't have multiple TMs at the same time (too much memory)
#if defined USE_TL2_ORIG
#include "stms/TL2Orig.hpp"
#define DATA_FILENAME "data/set-btree-1m-tl2orig.txt"
#elif defined USE_TL2_REDO
#include "stms/TL2Redo.hpp"
#define DATA_FILENAME "data/set-btree-1m-tl2redo.txt"
#elif defined USE_TL2_UNDO
#include "stms/TL2Undo.hpp"
#define DATA_FILENAME "data/set-btree-1m-tl2undo.txt"
#elif defined USE_TINY
#include "stms/TinySTM.hpp"
#define DATA_FILENAME "data/set-btree-1m-tiny.txt"
#elif defined USE_TL2LR
#include "stms/TL2LR.hpp"
#define DATA_FILENAME "data/set-btree-1m-tl2lr.txt"
#elif defined USE_OFWF
#include "stms/OneFileWF.hpp"
#define DATA_FILENAME "data/set-btree-1m-ofwf.txt"
#elif defined USE_FREEDAP
#include "stms/FreeDAP.hpp"
#define DATA_FILENAME "data/set-btree-1m-freedap.txt"
#elif defined USE_ROM_LR
#include "stms/romuluslr/RomulusLR.hpp"
#define DATA_FILENAME "data/set-btree-1m-romlr.txt"
#elif defined USE_OMEGA_L
#include "stms/OmegaL.hpp"
#define DATA_FILENAME "data/set-btree-1m-omegal.txt"
#elif defined USE_LL_FREE
#include "stms/LiveLockFree.hpp"
#define DATA_FILENAME "data/set-btree-1m-llfree.txt"
#elif defined USE_PRWLOCK
#include "stms/singlewriter/PRWLockSTM.hpp"
#define DATA_FILENAME "data/set-btree-1m-prwlock.txt"
#elif defined USE_SIM_RWLOCK
#include "stms/singlewriter/SimRWLock.hpp"
#define DATA_FILENAME "data/set-btree-1m-simrwlock.txt"
#elif defined USE_SIM_RWLOCK_FC
#include "stms/singlewriter/SimRWLockFC.hpp"
#define DATA_FILENAME "data/set-btree-1m-simrwlockfc.txt"
#elif defined USE_2PL_UNDO
#include "stms/2PLUndo.hpp"
#define DATA_FILENAME "data/set-btree-1m-2plundo.txt"
#elif defined USE_2PL_UNDO_DIST
#include "stms/2PLUndoDist.hpp"
#define DATA_FILENAME "data/set-btree-1m-2plundodist.txt"
#elif defined USE_2PLSF
#include "stms/2PLSF.hpp"
#define DATA_FILENAME "data/set-btree-1m-2plsf.txt"
#elif defined USE_DZ_TL2_SF
#include "stms/DualZoneTL2SF.hpp"
#define DATA_FILENAME "data/set-btree-1m-dztl2sf.txt"
#elif defined USE_TL2
#include "stms/zardoshti/tl2_wrap.hpp"
#define DATA_FILENAME "data/set-btree-1m-tl2.txt"
#elif defined USE_TLRW_EAGER
#include "stms/zardoshti/tlrw_eager_wrap.hpp"
#define DATA_FILENAME "data/set-btree-1m-tlrweager.txt"
#elif defined USE_OREC_EAGER
#include "stms/zardoshti/orec_eager_wrap.hpp"
#define DATA_FILENAME "data/set-btree-1m-oreceager.txt"
#elif defined USE_OREC_LAZY
#include "stms/zardoshti/orec_lazy_wrap.hpp"
#define DATA_FILENAME "data/set-btree-1m-oreclazy.txt"
#endif

#include "BenchmarkSets.hpp"


//
// Use like this:
// # bin/set-ravl-1m-blabla --keys=1000 --duration=2 --runs=1 --threads=1,2,4 --ratios=1000,100,0
//
int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    const std::string dataFilename { DATA_FILENAME };
    // Use 96 for c5.24xlarge   { 1, 2, 3, 4, 8, 12, 14, 16/*, 24, 32, 36, 40, 48, 64, 80, 96, 128*/ }
    vector<int> threadList = cfg.threads;
    vector<int> ratioList = cfg.ratios;
    const long numElements = cfg.keys;                               // Number of keys in the set
    const seconds testLength {cfg.duration};                         // 20s for the paper
    const int numRuns = cfg.runs;                                    // 5 runs for the paper
    uint64_t results[threadList.size()][ratioList.size()];
    std::string cName;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*threadList.size()*ratioList.size());

    // Sets benchmarks
    std::cout << "This benchmark takes about " << (threadList.size()*ratioList.size()*numRuns*testLength.count()/(60*60.)) << " hours to complete\n";
    std::cout << "\n----- Set Benchmark (B-Tree) -----\n";
    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (int it = 0; it < threadList.size(); it++) {
            int nThreads = threadList[it];
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (B-Trees)   keys=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined USE_TL2_ORIG
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tl2orig::TL2,tl2orig::tmtype>, uint64_t, tl2orig::TL2>                  (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TL2LR
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tl2lr::TL2LR,tl2lr::tmtype>, uint64_t, tl2lr::TL2LR>                    (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TL2_REDO
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tl2redo::TL2,tl2redo::tmtype>, uint64_t, tl2redo::TL2>                  (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TL2_UNDO
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tl2undo::TL2,tl2undo::tmtype>, uint64_t, tl2undo::TL2>                  (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_2PL_UNDO
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,twoplundo::STM,twoplundo::tmtype>, uint64_t, twoplundo::STM>            (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_2PL_UNDO_DIST
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,twoplundodist::STM,twoplundodist::tmtype>, uint64_t, twoplundodist::STM>(cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_2PLSF
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,twoplsf::STM,twoplsf::tmtype>, uint64_t, twoplsf::STM>(cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_DZ_TL2_SF
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,dztl2sf::STM,dztl2sf::tmtype>, uint64_t, dztl2sf::STM>                  (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TL2
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tl2::STM,tl2::tmtype>, uint64_t, tl2::STM>                              (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TLRW_EAGER
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tlrw_eager::STM,tlrw_eager::tmtype>, uint64_t, tlrw_eager::STM>         (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_OREC_EAGER
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,orec_eager::STM,orec_eager::tmtype>, uint64_t, orec_eager::STM>         (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_OREC_LAZY
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,orec_lazy::STM,orec_lazy::tmtype>, uint64_t, orec_lazy::STM>            (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_OFWF
            results[it][ir] = bench.benchmark<TMBTree<uint64_t,ofwf::STM,ofwf::tmtype>, uint64_t, ofwf::STM>                                (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_FREEDAP
            results[it][ir] = bench.benchmark<TMBTree<uint64_t,freedap::FreeDAP,freedap::tmtype>, uint64_t, freedap::FreeDAP>               (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_TINY
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,tinystm::TinySTM,tinystm::tmtype>, uint64_t, tinystm::TinySTM>          (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_ROM_LR
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,romuluslr::RomulusLR,romuluslr::tmtype>, uint64_t, romuluslr::RomulusLR>(cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_OMEGA_L
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,omegal::STM,omegal::tmtype>, uint64_t, omegal::STM>                     (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_LL_FREE
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,llfree::STM,llfree::tmtype>, uint64_t, llfree::STM>                     (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_PRWLOCK
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,prwlockstm::STM,prwlockstm::tmtype>, uint64_t, prwlockstm::STM>         (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_SIM_RWLOCK
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,simrwlock::STM,simrwlock::tmtype>, uint64_t, simrwlock::STM>            (cName, ratio, testLength, numRuns, numElements, false);
#elif defined USE_SIM_RWLOCK_FC
            results[it][ir] = bench.benchmark<TMBTreeByRef<uint64_t,simrwlockfc::STM,simrwlockfc::tmtype>, uint64_t, simrwlockfc::STM>      (cName, ratio, testLength, numRuns, numElements, false);
#else
            printf("ERROR: forgot to set a define?\n");
#endif
        }
        std::cout << "\n";
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and ratios for each column
    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        dataFile << cName << "-" << ratio/10. << "%"<< "\t";
    }
    dataFile << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (unsigned ir = 0; ir < ratioList.size(); ir++) {
            dataFile << results[it][ir] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
