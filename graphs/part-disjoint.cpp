#include <iostream>
#include <fstream>
#include <cstring>

#include "common/CmdLineConfig.hpp"
#include "BenchmarkPartDisjoint.hpp"

// Macros suck, but we can't have multiple TMs at the same time (too much memory)
#if defined USE_TL2_ORIG
#include "stms/TL2Orig.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2orig.txt"
#elif defined USE_TL2_REDO
#include "stms/TL2Redo.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2redo.txt"
#elif defined USE_TL2_REDO_GAP
#include "stms/TL2RedoGap.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2redogap.txt"
#elif defined USE_TL2_UNDO
#include "stms/TL2Undo.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2undo.txt"
#elif defined USE_TL2_UNDO_MVCC
#include "stms/TL2UndoMVCC.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2undomvcc.txt"
#elif defined USE_TL2_UNDO_GAP
#include "stms/TL2UndoGap.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2undogap.txt"
#elif defined USE_TL2_UNDO_GAP_WP
#include "stms/TL2UndoGapWP.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2undogapwp.txt"
#elif defined USE_TINY
#include "stms/TinySTM.hpp"
#define DATA_FILENAME "data/part-disjoint-tiny.txt"
#elif defined USE_TL2LR
#include "stms/TL2LR.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2lr.txt"
#elif defined USE_2PL_UNDO
#include "stms/2PLUndo.hpp"
#define DATA_FILENAME "data/part-disjoint-2plundo.txt"
#elif defined USE_2PL_UNDO_DIST
#include "stms/2PLUndoDist.hpp"
#define DATA_FILENAME "data/part-disjoint-2plundodist.txt"
#elif defined USE_2PLSF
#include "stms/2PLSF.hpp"
#define DATA_FILENAME "data/part-disjoint-2plsf.txt"
#elif defined USE_OREC_EAGER
#include "stms/zardoshti/orec_eager_wrap.hpp"
#define DATA_FILENAME "data/part-disjoint-oreceager.txt"
#elif defined USE_DZ_TL2_SF
#include "stms/DualZoneTL2SF.hpp"
#define DATA_FILENAME "data/part-disjoint-dztl2sf.txt"
#elif defined USE_OFWF
#include "stms/OneFileWF.hpp"
#define DATA_FILENAME "data/part-disjoint-ofwf.txt"
#elif defined USE_FREEDAP
#include "stms/FreeDAP.hpp"
#define DATA_FILENAME "data/part-disjoint-freedap.txt"
#elif defined USE_ROM_LR
#include "stms/romuluslr/RomulusLR.hpp"
#define DATA_FILENAME "data/part-disjoint-romlr.txt"
#elif defined USE_CRWWP
#include "stms/CRWWPSTM.hpp"
#define DATA_FILENAME "data/part-disjoint-crwwp.txt"
#elif defined USE_PRWLOCK
#include "pdatastructures/PRWLockRAVLSet.hpp"
#include "stms/singlewriter/PRWLockSTM.hpp"
#define DATA_FILENAME "data/part-disjoint-prwlock.txt"
#elif defined USE_OQLF
#include "stms/OneQueueLF.hpp"
#define DATA_FILENAME "data/part-disjoint-oqlf.txt"
#elif defined USE_OMEGA
#include "stms/Omega.hpp"
#define DATA_FILENAME "data/part-disjoint-omega.txt"
#elif defined USE_OMEGA_O
#include "stms/OmegaO.hpp"
#define DATA_FILENAME "data/part-disjoint-omegao.txt"
#elif defined USE_OMEGA_L
#include "stms/OmegaL.hpp"
#define DATA_FILENAME "data/part-disjoint-omegal.txt"
#elif defined USE_UNDERLAY
#include "stms/UnderlaySnapshot.hpp"
#define DATA_FILENAME "data/part-disjoint-underlay.txt"
#elif defined USE_UNDERLAY_GROUP
#include "stms/UnderlayGroup.hpp"
#define DATA_FILENAME "data/part-disjoint-underlaygroup.txt"
#elif defined USE_1PL_OCC_UNDO
#include "stms/1PLOCCUndo.hpp"
#define DATA_FILENAME "data/part-disjoint-1ploccundo.txt"
#elif defined USE_LL_FREE
#include "stms/LiveLockFree.hpp"
#define DATA_FILENAME "data/part-disjoint-llfree.txt"
#elif defined USE_ICCC_UNDO_B
#include "stms/ICCCUndoB.hpp"
#define DATA_FILENAME "data/part-disjoint-icccundob.txt"
#elif defined USE_ICCC_UNDO_E
#include "stms/ICCCUndoE.hpp"
#define DATA_FILENAME "data/part-disjoint-icccundoe.txt"
#elif defined USE_RTL_UNDO
#include "stms/RTLUndo.hpp"
#define DATA_FILENAME "data/part-disjoint-rtlundo.txt"
#elif defined USE_SIM_RWLOCK
#include "stms/singlewriter/SimRWLock.hpp"
#define DATA_FILENAME "data/part-disjoint-simrwlock.txt"
#elif defined USE_SIM_RWLOCK_FC
#include "stms/singlewriter/SimRWLockFC.hpp"
#define DATA_FILENAME "data/part-disjoint-simrwlockfc.txt"
#elif defined USE_TL2_REDO_MAP
#include "stms/doublemap/TL2RedoMap.hpp"
#define DATA_FILENAME "data/part-disjoint-tl2redomap.txt"
#endif


//
// Use like this:
// # bin/part-disjoint-blabla --duration=2 --runs=1 --threads=1,2,4
//
int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    const std::string dataFilename { DATA_FILENAME };
    // Use 96 for c5.24xlarge   { 1, 2, 3, 4, 8, 12, 14, 16/*, 24, 32, 36, 40, 48, 64, 80, 96, 128*/ }
    vector<int> threadList = cfg.threads;
    const long numElements = cfg.keys;                               // Number of keys in the set
    const seconds testLength {cfg.duration};                         // 20s for the paper
    const int numRuns = cfg.runs;                                    // 5 runs for the paper
    uint64_t results[threadList.size()];
    std::string cName;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*threadList.size());

    std::cout << "This benchmark takes about " << (threadList.size()*numRuns*testLength.count()/(60*60.)) << " hours to complete\n";
    std::cout << "\n----- Partially Disjoint Benchmark -----\n";
    for (int it = 0; it < threadList.size(); it++) {
        int nThreads = threadList[it];
        BenchmarkPartDisjoint bench(nThreads);
        std::cout << "\n----- Pair threads in opposite directions   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined USE_TL2_ORIG
        results[it] = bench.benchmark<tl2orig::TL2,tl2orig::tmtype>                       (cName, testLength, numRuns);
#elif defined USE_TL2LR
        results[it] = bench.benchmark<tl2lr::TL2LR,tl2lr::tmtype>                         (cName, testLength, numRuns);
#elif defined USE_TL2_REDO
        results[it] = bench.benchmark<tl2redo::TL2,tl2redo::tmtype>                       (cName, testLength, numRuns);
#elif defined USE_TL2_REDO_GAP
        results[it] = bench.benchmark<tl2redogap::TL2,tl2redogap::tmtype>                 (cName, testLength, numRuns);
#elif defined USE_TL2_UNDO
        results[it] = bench.benchmark<tl2undo::TL2,tl2undo::tmtype>                       (cName, testLength, numRuns);
#elif defined USE_TL2_UNDO_MVCC
        results[it] = bench.benchmark<tl2undomvcc::TL2,tl2undomvcc::tmtype>               (cName, testLength, numRuns);
#elif defined USE_TL2_UNDO_GAP
        results[it] = bench.benchmark<tl2undogap::TL2,tl2undogap::tmtype>                 (cName, testLength, numRuns);
#elif defined USE_TL2_UNDO_GAP_WP
        results[it] = bench.benchmark<tl2undogapwp::TL2,tl2undogapwp::tmtype>             (cName, testLength, numRuns);
#elif defined USE_2PL_UNDO
        results[it] = bench.benchmark<twoplundo::STM,twoplundo::tmtype>                   (cName, testLength, numRuns);
#elif defined USE_2PL_UNDO_DIST
        results[it] = bench.benchmark<twoplundodist::STM,twoplundodist::tmtype>           (cName, testLength, numRuns);
#elif defined USE_2PLSF
        results[it] = bench.benchmark<twoplsf::STM,twoplsf::tmtype>       (cName, testLength, numRuns);
#elif defined USE_OREC_EAGER
        results[it] = bench.benchmark<orec_eager::STM,orec_eager::tmtype>                 (cName, testLength, numRuns);
#elif defined USE_DZ_TL2_SF
        results[it] = bench.benchmark<dztl2sf::STM,dztl2sf::tmtype>                       (cName, testLength, numRuns);
#elif defined USE_OFWF
        results[it] = bench.benchmark<ofwf::STM,ofwf::tmtype>                             (cName, testLength, numRuns);
#elif defined USE_TINY
        results[it] = bench.benchmark<tinystm::TinySTM,tinystm::tmtype>                   (cName, testLength, numRuns);
#elif defined USE_ROM_LR
        results[it] = bench.benchmark<romuluslr::RomulusLR,romuluslr::tmtype>             (cName, testLength, numRuns);
#elif defined USE_CRWWP
        results[it] = bench.benchmark<crwwpstm::CRWWPSTM,crwwpstm::tmtype>                (cName, testLength, numRuns);
#elif defined USE_OMEGA
        results[it] = bench.benchmark<omega::STM,omega::tmtype>                           (cName, testLength, numRuns);
#elif defined USE_UNDERLAY
        results[it] = bench.benchmark<underlay::UnderlaySnapshot,underlay::tmtype>        (cName, testLength, numRuns);
#elif defined USE_UNDERLAY_GROUP
        results[it] = bench.benchmark<underlaygroup::UnderlaySnapshot,underlaygroup::tmtype>(cName, testLength, numRuns);
#elif defined USE_1PL_OCC_UNDO
        results[it] = bench.benchmark<oneploccundo::STM,oneploccundo::tmtype>             (cName, testLength, numRuns);
#elif defined USE_LL_FREE
        results[it] = bench.benchmark<llfree::STM,llfree::tmtype>                         (cName, testLength, numRuns);
#elif defined USE_ICCC_UNDO_B
        results[it] = bench.benchmark<icccundob::STM,icccundob::tmtype>                   (cName, testLength, numRuns);
#elif defined USE_ICCC_UNDO_E
        results[it] = bench.benchmark<icccundoe::STM,icccundoe::tmtype>                   (cName, testLength, numRuns);
#elif defined USE_RTL_UNDO
        results[it] = bench.benchmark<rtlundo::STM,rtlundo::tmtype>                       (cName, testLength, numRuns);
#elif defined USE_SIM_RWLOCK
        results[it] = bench.benchmark<simrwlock::STM,simrwlock::tmtype>                   (cName, testLength, numRuns);
#elif defined USE_SIM_RWLOCK_FC
        results[it] = bench.benchmark<simrwlockfc::STM,simrwlockfc::tmtype>               (cName, testLength, numRuns);
#elif defined USE_TL2_REDO_MAP
        results[it] = bench.benchmark<tl2redomap::TL2,tl2redomap::tmtype>                 (cName, testLength, numRuns);
#else
        printf("ERROR: forgot to set a define?\n");
#endif
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names
    dataFile << cName << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        dataFile << results[it] << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
