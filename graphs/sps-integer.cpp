/*
 * Executes SPS benchmark
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "common/CmdLineConfig.hpp"
#include "BenchmarkSPS.hpp"
// Macros suck, but we can't have multiple TMs at the same time (too much memory)
#if defined USE_TL2_ORIG
#include "stms/TL2Orig.hpp"
#define DATA_FILENAME "data/sps-integer-tl2orig.txt"
#elif defined USE_TL2_REDO
#include "stms/TL2Redo.hpp"
#define DATA_FILENAME "data/sps-integer-tl2redo.txt"
#elif defined USE_TL2_REDO_GAP
#include "stms/TL2RedoGap.hpp"
#define DATA_FILENAME "data/sps-integer-tl2redogap.txt"
#elif defined USE_TL2_UNDO
#include "stms/TL2Undo.hpp"
#define DATA_FILENAME "data/sps-integer-tl2undo.txt"
#elif defined USE_TL2_UNDO_GAP
#include "stms/TL2UndoGap.hpp"
#define DATA_FILENAME "data/sps-integer-tl2undogap.txt"
#elif defined USE_TL2_UNDO_GAP_WP
#include "stms/TL2UndoGapWP.hpp"
#define DATA_FILENAME "data/sps-integer-tl2undogapwp.txt"
#elif defined USE_RTL
#include "stms/RTL.hpp"
#define DATA_FILENAME "data/sps-integer-rtl.txt"
#elif defined USE_TINY
#include "stms/TinySTM.hpp"
#define DATA_FILENAME "data/sps-integer-tiny.txt"
#elif defined USE_TL2LR
#include "stms/TL2LR.hpp"
#define DATA_FILENAME "data/sps-integer-tl2lr.txt"
#elif defined USE_2PL_UNDO
#include "stms/2PLUndo.hpp"
#define DATA_FILENAME "data/sps-integer-2plundo.txt"
#elif defined USE_2PL_UNDO_DIST
#include "stms/2PLUndoDist.hpp"
#define DATA_FILENAME "data/sps-integer-2plundodist.txt"
#elif defined USE_2PLSF
#include "stms/2PLSF.hpp"
#define DATA_FILENAME "data/sps-integer-2plsf.txt"
#elif defined USE_OREC_EAGER
#include "stms/zardoshti/orec_eager_wrap.hpp"
#define DATA_FILENAME "data/sps-integer-oreceager.txt"
#elif defined USE_OREC_LAZY
#include "stms/zardoshti/orec_lazy_wrap.hpp"
#define DATA_FILENAME "data/sps-integer-oreclazy.txt"
#elif defined USE_TL2
#include "stms/zardoshti/tl2_wrap.hpp"
#define DATA_FILENAME "data/sps-integer-tl2.txt"
#elif defined USE_TLRW_EAGER
#include "stms/zardoshti/tlrw_eager_wrap.hpp"
#define DATA_FILENAME "data/sps-integer-tlrweager.txt"
#elif defined USE_OFWF
#include "stms/OneFileWF.hpp"
#define DATA_FILENAME "data/sps-integer-ofwf.txt"
#elif defined USE_FREEDAP
#include "stms/FreeDAP.hpp"
#define DATA_FILENAME "data/sps-integer-freedap.txt"
#elif defined USE_ROM_LR
#include "stms/romuluslr/RomulusLR.hpp"
#define DATA_FILENAME "data/sps-integer-romlr.txt"
#elif defined USE_CRWWP
#include "stms/CRWWPSTM.hpp"
#define DATA_FILENAME "data/sps-integer-crwwp.txt"
#elif defined USE_PRWLOCK
#include "stms/PRWLockSTM.hpp"
#define DATA_FILENAME "data/sps-integer-prwlock.txt"
#elif defined USE_OQLF
#include "stms/OneQueueLF.hpp"
#define DATA_FILENAME "data/sps-integer-oqlf.txt"
#elif defined USE_OMEGA_L
#include "stms/OmegaL.hpp"
#define DATA_FILENAME "data/sps-integer-omegal.txt"
#elif defined USE_1PL_OCC_UNDO
#include "stms/1PLOCCUndo.hpp"
#define DATA_FILENAME "data/sps-integer-1ploccundo.txt"
#elif defined USE_2PL_UNDO
#include "stms/2PLUndo.hpp"
#define DATA_FILENAME "data/sps-integer-2plundo.txt"
#elif defined USE_LL_FREE
#include "stms/LiveLockFree.hpp"
#define DATA_FILENAME "data/sps-integer-llfree.txt"
#endif


int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    const std::string dataFilename { DATA_FILENAME };
    vector<int> threadList = cfg.threads;
    vector<long> swapsPerTxList = { 2, 32, 128 }; // Number of swapped words per transaction
    const seconds testLength {cfg.duration};                         // 20s for the paper
    const int numRuns = cfg.runs;                                    // 5 runs for the paper
    uint64_t results[threadList.size()][swapsPerTxList.size()];
    std::string cName;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*threadList.size()*swapsPerTxList.size());

    // SPS Benchmarks multi-threaded
    std::cout << "This benchmark takes about " << (threadList.size()*swapsPerTxList.size()*numRuns*testLength.count()/(60*60.)) << " hours to complete\n";
    std::cout << "\n----- SPS Benchmark (multi-threaded integer array swap) -----\n";
    for (int is = 0; is < swapsPerTxList.size(); is++) {
        int nWords = swapsPerTxList[is];
        for (int it = 0; it < threadList.size(); it++) {
            int nThreads = threadList[it];
            BenchmarkSPS bench(nThreads);
            std::cout << "\n----- threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s   arraySize=" << arraySize << "   swaps/tx=" << nWords << " -----\n";
#if defined USE_TL2_ORIG
            results[it][is] = bench.benchmarkSPSInteger<tl2orig::TL2,tl2orig::tmtype>                  (cName, testLength, nWords, numRuns);
#elif defined USE_TL2_REDO
            results[it][is] = bench.benchmarkSPSInteger<tl2redo::TL2,tl2redo::tmtype>                  (cName, testLength, nWords, numRuns);
#elif defined USE_TL2_UNDO
            results[it][is] = bench.benchmarkSPSInteger<tl2undo::TL2,tl2undo::tmtype>                  (cName, testLength, nWords, numRuns);
#elif defined USE_2PL_UNDO
            results[it][is] = bench.benchmarkSPSInteger<twoplundo::STM,twoplundo::tmtype>              (cName, testLength, nWords, numRuns);
#elif defined USE_2PL_UNDO_DIST
            results[it][is] = bench.benchmarkSPSInteger<twoplundodist::STM,twoplundodist::tmtype>      (cName, testLength, nWords, numRuns);
#elif defined USE_2PLSF
            results[it][is] = bench.benchmarkSPSInteger<twoplsf::STM,twoplsf::tmtype>                  (cName, testLength, nWords, numRuns);
#elif defined USE_OREC_EAGER
            results[it][is] = bench.benchmarkSPSInteger<orec_eager::STM,orec_eager::tmtype>            (cName, testLength, nWords, numRuns);
#elif defined USE_OREC_LAZY
            results[it][is] = bench.benchmarkSPSInteger<orec_lazy::STM,orec_lazy::tmtype>              (cName, testLength, nWords, numRuns);
#elif defined USE_TL2
            results[it][is] = bench.benchmarkSPSInteger<tl2::STM,tl2::tmtype>                          (cName, testLength, nWords, numRuns);
#elif defined USE_TLRW_EAGER
            results[it][is] = bench.benchmarkSPSInteger<tlrw_eager::STM,tlrw_eager::tmtype>            (cName, testLength, nWords, numRuns);
#elif defined USE_OFWF
            results[it][is] = bench.benchmarkSPSInteger<ofwf::STM, ofwf::tmtype>                       (cName, testLength, nWords, numRuns);
#elif defined USE_TINY
            results[it][is] = bench.benchmarkSPSInteger<tinystm::TinySTM,tinystm::tmtype>              (cName, testLength, nWords, numRuns);
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
    // Printf class names for each column plus the corresponding thread
    for (unsigned is = 0; is < swapsPerTxList.size(); is++) {
        int nWords = swapsPerTxList[is];
        dataFile << cName << "-" << nWords <<"\t";
    }
    dataFile << "\n";
    for (unsigned it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (unsigned is = 0; is < swapsPerTxList.size(); is++) {
            dataFile << results[it][is] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
