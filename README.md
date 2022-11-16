# 2PLSF - Two-Phase Locking with Starvation-Freedom

This repository contains the source code and benchmarks of the paper:

Pedro Ramalhete, Andreia Correia, Pascal Felber, [2PLSF - Two-Phase Locking with Starvation-Freedom](draft not yet available), PPoPP 2023

The source code the 2PLSF is located in stms/2PLSF.hpp and no other file is required to use this concurrency control in your work.

## STMs
This repository contains the following STM implementations in the stms/ folder:

- TinySTM.hpp and tinystm/			&nbsp;&nbsp; Wrapper of TinySTM with the LSA concurrency control
- TL2ORig.hpp and tl2-x86/			&nbsp;&nbsp; Wrapper of the original TL2 concurrency control implementation
- zardoshti/orec_eager_wrap.hpp		&nbsp;&nbsp; Wrapper of the OREC eager STM
- zardoshti/orec_lazy_wrap.hpp		&nbsp;&nbsp; Wrapper of the OREC lazy STM
- zardoshti/tl2_wrap.hpp			&nbsp;&nbsp; Wrapper of the TL2 implementation by Zardoshti et al
- zardoshti/tlrw_eager_wrap.hpp		&nbsp;&nbsp; Wrapper of the TLRW implementation by Zardoshti et al
- OneFileWF.hpp						&nbsp;&nbsp; OneFile Wait-Free STM
- 2PLSF.hpp							&nbsp;&nbsp; 2PLSF Software Transactional Memory
- 2PLUndo.hpp						&nbsp;&nbsp; 2PL with Undo and non-scalable reader-writer lock
- 2PLUndoDist.hpp					&nbsp;&nbsp; 2PL with Undo and scalable reader-writer lock

We use wrapper classes to integrate some of the existing STMs with our benchmarks.



## Data structure benchmarks
The folder graphs/ contains the benchmarks for data structures.

### Build data structure benchmarks
To build you must first build the TinySTM:

	cd stms/tinystm
	make

then build TL2

	cd ../tl2-x86
	make
	
then build the data structures benchmarks:

	cd ../../graphs
	make

If you don't have g++-10 then edit the Makefile in the folder above and replace CXX with the compiler you wish to use.

### Run data structure benchmarks
To run the benchmarks you can go into the graphs/ folder and run the python script run-all.py

	python2 run-all.py


## DBx1000 benchmarks
The source code for the DBx1000 benchmark was taken from https://github.com/yxymit/DBx1000
We fixed a couple of issues but the inner workings stayed the same. We added the 2PLSF concurrency control, with the file stms/2PLSF.hpp being needed in addition to the files added in DBx1000/

We have also disabled the abort buffer as it will not work properly with 2PLSF because 2PLSF waits for a specific thread/transaction to complete.
You can check which settings to use by looking at run-dbx1000.py or executing this script to generate YCSB results for the currently selected Concurrency Control (CC).
To chose a different CC, open DBx1000/config.h and replace CC_ALG with one of the other names: WAIT_DIE, NO_WAIT, DL_DETECT, SILO, HEKATON, TICTOC, TWO_PL_SF

2PLSF will disregard the ABORT_PENALTY because it uses its own restarting scheme.

We also added a "continue" in the main loop which causes NO_WAIT to get stuck due to permanent conflicts aborting themselves. This happens because NO_WAIT is prone to live-lock, it's just that the DBx1000 benchmarks were originally made so that this would not be seen.

Keep in mind that this particular implementation of DBx1000 does no exercise the indexing data structure, as the index is a sequential hashmap. In other words, the YCSB of this benchmark is *not* the true YCSB because there are no record insertions nor deletions, only record updates of pre-existing records.
For slightly more realistic YCSB benchmarks, take a look at setbench: https://gitlab.com/trbot86/setbench/-/wikis/home

After editing the config.h, build with

	make -j
and run with
	
	./rundb
or

	./run-dbx1000.py



## Figures
After running the benchmarks, it's possible to obtain the figures from the paper by going in the graphs/plots folder and running ./plot-all.sh
However, the DBx1000 results need to be copied one by one to the appropriate files in the graphs/data/cervino/ folder, as this benchmark is not easily automated.
The plot-all.sh script requires a recent gnuplot and a few other tools to run correctly, like ps2pdf.


## How to use 2PLSF in a different benchmark
To use 2PLSF in your own application or benchmark, include the 2PLSF.hpp header in one of your .cpp files. You will not need any other file.
If for some reason you need to include it from multiple .cpp files, then #define INCLUDED_FROM_MULTIPLE_CPP before the #include<2PLSF.hpp>. Do this from all the .cpp files except one, otherwise you will see multiple declarations of the globals.
Alternatively, you can move the last block of code in 2PLSF.hpp to one of your .cpp files, and then include from all the .cpp files without the extra #define.

You can start transactions by passing a lambda to twoplsf::transaction() with txType=TX_IS_UPDATE for write-transactions and txType=TX_IS_READ for read-transactions. Read accesses should be done through tmtype::pload() and write access through tmtype::pstore().


For further questions go to 
http://www.github.com/pramalhe/2PLSF.git