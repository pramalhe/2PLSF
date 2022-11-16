#include <iostream>
#include <fstream>
#include <cstring>

#include "common/CmdLineConfig.hpp"
#include "pdatastructures/maps/TMSkipListMap.hpp"
#include "pdatastructures/maps/TMSkipListMapByRef.hpp"

static const int RECORD_SIZE = 12;            // The default Value is a "Record" with size 96 bytes, where each 8 byte is a word

// Macros suck, but we can't have multiple TMs at the same time (too much memory)
#if defined USE_TL2_ORIG
#include "stms/TL2Orig.hpp"
struct Record  { tl2orig::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2orig.txt"
#elif defined USE_TL2_REDO
#include "stms/TL2Redo.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2redo.txt"
#elif defined USE_TL2_REDO_GAP
#include "stms/TL2RedoGap.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2redogap.txt"
#elif defined USE_TL2_UNDO
#include "stms/TL2Undo.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2undo.txt"
#elif defined USE_TL2_UNDO_MVCC
#include "stms/TL2UndoMVCC.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2undomvcc.txt"
#elif defined USE_TL2_UNDO_GAP
#include "stms/TL2UndoGap.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2undogap.txt"
#elif defined USE_TL2_UNDO_GAP_WP
#include "stms/TL2UndoGapWP.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2undogapwp.txt"
#elif defined USE_TINY
#include "stms/TinySTM.hpp"
struct Record  { tinystm::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-tiny.txt"
#elif defined USE_2PL_UNDO
#include "stms/2PLUndo.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-2plundo.txt"
#elif defined USE_2PL_UNDO_DIST
#include "stms/2PLUndoDist.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-2plundodist.txt"
#elif defined USE_2PLSF
#include "stms/2PLSF.hpp"
struct Record  { twoplsf::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-2plsf.txt"
#elif defined USE_DZ_TL2_SF
#include "stms/DualZoneTL2SF.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-dztl2sf.txt"
#elif defined USE_OREC_EAGER
#include "stms/zardoshti/orec_eager_wrap.hpp"
struct Record  { orec_eager::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-oreceager.txt"
#elif defined USE_OREC_LAZY
#include "stms/zardoshti/orec_lazy_wrap.hpp"
struct Record  { orec_lazy::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-oreclazy.txt"
#elif defined USE_OFWF
#include "stms/OneFileWF.hpp"
struct Record : public ofwf::tmbase { ofwf::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-ofwf.txt"
#elif defined USE_OMEGA
#include "stms/Omega.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-omega.txt"
#elif defined USE_TL2
#include "stms/zardoshti/tl2_wrap.hpp"
#define DATA_FILENAME "data/map-skiplist-98u-100k-tl2.txt"
#elif defined USE_TLRW_EAGER
#include "stms/zardoshti/tlrw_eager_wrap.hpp"
struct Record  { tlrw_eager::tmtype<uint64_t> data[RECORD_SIZE]; };
#define DATA_FILENAME "data/map-skiplist-98u-100k-tlrweager.txt"
#endif


#include "BenchmarkMaps.hpp"


//
// Use like this:
// # bin/map-ravl-10k-blabla --keys=1000 --duration=2 --runs=1 --threads=1,2,4 --ratios=1000,100,0
//
int main(int argc, char* argv[]) {
    CmdLineConfig cfg;
    cfg.parseCmdLine(argc,argv);
    cfg.print();

    uint64_t rqSize = cfg.rqsize;
    std::string dataFilename { DATA_FILENAME };
    //if (rqSize != 0) dataFilename.insert(dataFilename.size()-3, std::to_string(rqSize));
    //printf("filename will be %s\n", dataFilename.c_str());
    // Use 96 for c5.24xlarge   { 1, 2, 3, 4, 8, 12, 14, 16/*, 24, 32, 36, 40, 48, 64, 80, 96, 128*/ }
    vector<int> threadList = cfg.threads;
    vector<int> ratioList = cfg.ratios;
    const long numKeys = cfg.keys;                               // Number of keys in the set
    const seconds testLength {cfg.duration};                         // 20s for the paper
    const int numRuns = cfg.runs;                                    // 5 runs for the paper
    const bool doDedicated = false;
    uint64_t results[threadList.size()][ratioList.size()];
    std::string cName;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*threadList.size()*ratioList.size());
    // TODO: pass these ratios in the command-line
    // Ratios
    int insertRatio = 10;
    int removeRatio = 10;
    int updateRatio = 980;
    int rqRatio = 0;

    // Sets benchmarks
    if (doDedicated) printf("Running with two DEDICATED writer threads enabled\n");
    if (rqSize != 0) printf("Running with RANGE QUERIES enabled   rqsize=%ld\n", rqSize);
    std::cout << "This benchmark takes about " << (threadList.size()*ratioList.size()*numRuns*testLength.count()/(60*60.)) << " hours to complete\n";
    std::cout << "\n----- Map Benchmark (SkipList) -----\n";
    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (int it = 0; it < threadList.size(); it++) {
            int nThreads = threadList[it];
            BenchmarkMaps bench(nThreads);
            std::cout << "\n----- Maps (Skiplist)   keys=" << numKeys << "  i=" << insertRatio/10. << "% r=" << removeRatio/10. << "% u=" << updateRatio/10. << "% rq=" << rqRatio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined USE_TL2_ORIG
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2orig::TL2,tl2orig::tmtype>, tl2orig::TL2>            (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_REDO
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2redo::TL2,tl2redo::tmtype>, tl2redo::TL2>            (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_REDO_GAP
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2redogap::TL2,tl2redogap::tmtype>, tl2redogap::TL2>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_UNDO
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2undo::TL2,tl2undo::tmtype>, tl2undo::TL2>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_UNDO_MVCC
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2undomvcc::TL2,tl2undomvcc::tmtype>, tl2undomvcc::TL2>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_UNDO_GAP
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2undogap::TL2,tl2undogap::tmtype>, tl2undogap::TL2>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2_UNDO_GAP_WP
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2undogapwp::TL2,tl2undogapwp::tmtype>, tl2undogapwp::TL2>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_2PL_UNDO
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,twoplundo::STM,twoplundo::tmtype>, twoplundo::STM>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_2PL_UNDO_DIST
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,twoplundodist::STM,twoplundodist::tmtype>, twoplundodist::STM>                       (cName, ratio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_2PLSF
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,twoplsf::STM,twoplsf::tmtype>, twoplsf::STM> (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_DZ_TL2_SF
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,dztl2sf::STM,dztl2sf::tmtype>, dztl2sf::STM>            (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TL2
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tl2::STM,tl2::tmtype>, tl2::STM>                        (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TLRW_EAGER
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tlrw_eager::STM,tlrw_eager::tmtype>, tlrw_eager::STM>   (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_OREC_EAGER
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,orec_eager::STM,orec_eager::tmtype>, orec_eager::STM>   (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_OREC_LAZY
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,orec_lazy::STM,orec_lazy::tmtype>, orec_lazy::STM>      (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_TINY
            results[it][ir] = bench.benchmark<TMSkipListMapByRef<uint64_t,Record*,tinystm::TinySTM,tinystm::tmtype>, tinystm::TinySTM>    (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_OFWF
            results[it][ir] = bench.benchmark<TMSkipListMap<uint64_t,Record*,ofwf::STM,ofwf::tmtype>, ofwf::STM>                          (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
#elif defined USE_OMEGA
            results[it][ir] = bench.benchmark<TMSkipListMap<uint64_t,Record*,omega::STM,omega::tmtype>, omega::STM>                       (cName, insertRatio, removeRatio, updateRatio, rqRatio, testLength, numRuns, numKeys, doDedicated, rqSize);
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
