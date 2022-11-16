CXX = g++-10
CXXFLAGS = -std=c++14 -g -O2 #-fsanitize=address
INCLUDES = -I../ -I../common/ 

# This library is needed for ESTM
ESTM_LIB = -L../stms/estm-0.3.0/lib/ -lstm

# This library is needed for TinySTM
TINYSTM_LIB = -L../stms/tinystm/lib/ -lstm
TINYSTM_INC = -I../stms/tinystm/

# This library is the original TL2, available on github 
TL2_ORIG_LIB = -L../stms/tl2-x86/ -ltl2
# Building with the library is slow due to the interposing. For best results we should compile directly
TL2_CSRC = ../stms/tl2-x86/tl2.c ../stms/tl2-x86/tmalloc.c


BINARIES = \
	bin/part-disjoint-tl2orig \
	bin/part-disjoint-tiny \
	bin/part-disjoint-2plundo \
	bin/part-disjoint-2plundodist \
	bin/part-disjoint-2plsf \
	bin/part-disjoint-ofwf \
	bin/part-disjoint-oreceager \
	bin/set-btree-1m-tl2orig \
	bin/set-btree-1m-tiny \
	bin/set-btree-1m-2plundo \
	bin/set-btree-1m-2plundodist \
	bin/set-btree-1m-2plsf \
	bin/set-btree-1m-tl2 \
	bin/set-btree-1m-tlrweager \
	bin/set-btree-1m-oreceager \
	bin/set-btree-1m-oreclazy \
	bin/set-btree-1m-ofwf \
	bin/set-ll-1k-tl2orig \
	bin/set-ll-1k-tiny \
	bin/set-ll-1k-2plundo \
	bin/set-ll-1k-2plundodist \
	bin/set-ll-1k-2plsf \
	bin/set-ll-1k-tl2 \
	bin/set-ll-1k-tlrweager \
	bin/set-ll-1k-oreceager \
	bin/set-ll-1k-oreclazy \
	bin/set-ll-1k-ofwf \
	bin/set-ravl-1m-tl2orig \
	bin/set-ravl-1m-tiny \
	bin/set-ravl-1m-2plundo \
	bin/set-ravl-1m-2plundodist \
	bin/set-ravl-1m-2plsf \
	bin/set-ravl-1m-tl2 \
	bin/set-ravl-1m-tlrweager \
	bin/set-ravl-1m-oreceager \
	bin/set-ravl-1m-oreclazy \
	bin/set-ravl-1m-ofwf \
	bin/set-skiplist-1m-tl2orig \
	bin/set-skiplist-1m-tiny \
	bin/set-skiplist-1m-2plundo \
	bin/set-skiplist-1m-2plundodist \
	bin/set-skiplist-1m-2plsf \
	bin/set-skiplist-1m-tl2 \
	bin/set-skiplist-1m-tlrweager \
	bin/set-skiplist-1m-oreceager \
	bin/set-skiplist-1m-oreclazy \
	bin/set-skiplist-1m-ofwf \
	bin/set-tree-1m-tl2orig \
	bin/set-tree-1m-tiny \
	bin/set-tree-1m-2plundo \
	bin/set-tree-1m-2plundodist \
	bin/set-tree-1m-2plsf \
	bin/set-tree-1m-tl2 \
	bin/set-tree-1m-tlrweager \
	bin/set-tree-1m-oreceager \
	bin/set-tree-1m-oreclazy \
	bin/set-tree-1m-ofwf \
	bin/set-ziptree-1m-tl2orig \
	bin/set-ziptree-1m-tiny \
	bin/set-ziptree-1m-2plundo \
	bin/set-ziptree-1m-2plundodist \
	bin/set-ziptree-1m-2plsf \
	bin/set-ziptree-1m-tl2 \
	bin/set-ziptree-1m-tlrweager \
	bin/set-ziptree-1m-oreceager \
	bin/set-ziptree-1m-oreclazy \
	bin/set-ziptree-1m-ofwf \
	bin/set-hash-10k-tl2orig \
	bin/set-hash-10k-tiny \
	bin/set-hash-10k-2plundo \
	bin/set-hash-10k-2plundodist \
	bin/set-hash-10k-2plsf \
	bin/set-hash-10k-tl2 \
	bin/set-hash-10k-tlrweager \
	bin/set-hash-10k-oreceager \
	bin/set-hash-10k-oreclazy \
	bin/set-hash-10k-ofwf \
	bin/sps-integer-tl2orig \
	bin/sps-integer-tiny \
	bin/sps-integer-2plundo \
	bin/sps-integer-2plundodist \
	bin/sps-integer-2plsf \
	bin/sps-integer-tl2 \
	bin/sps-integer-tlrweager \
	bin/sps-integer-oreceager \
	bin/sps-integer-oreclazy \
	bin/sps-integer-ofwf \
	bin/map-ravl-tl2orig \
	bin/map-ravl-tiny \
	bin/map-ravl-2plsf \
	bin/map-ravl-tlrweager \
	bin/map-ravl-oreceager \
	bin/map-ravl-oreclazy \
	bin/map-ravl-ofwf \
	bin/map-skiplist-tl2orig \
	bin/map-skiplist-tiny \
	bin/map-skiplist-2plsf \
	bin/map-skiplist-tlrweager \
	bin/map-skiplist-oreceager \
	bin/map-skiplist-oreclazy \
	bin/map-skiplist-ofwf \
	bin/map-ziptree-tl2orig \
	bin/map-ziptree-tiny \
	bin/map-ziptree-2plsf \
	bin/map-ziptree-tlrweager \
	bin/map-ziptree-oreceager \
	bin/map-ziptree-oreclazy \
	bin/map-ziptree-ofwf \


		
all: $(BINARIES)


clean:
	rm -f bin/*
	rm -f lib/*




#
# Queues for volatile memory
#	
bin/q-ll-enq-deq: q-ll-enq-deq.cpp $(STMS) $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq -lpthread $(ESTM_LIB)
	
	
#	
# SPS for volatile memory
#
bin/sps-integer-tl2redo: sps-integer.cpp BenchmarkSPS.hpp ../stms/TL2Redo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_REDO $(INCLUDES) sps-integer.cpp -o bin/sps-integer-tl2redo -lpthread

bin/sps-integer-tl2orig: sps-integer.cpp BenchmarkSPS.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) sps-integer.cpp -o bin/sps-integer-tl2orig -lpthread

bin/sps-integer-ofwf: sps-integer.cpp BenchmarkSPS.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) sps-integer.cpp -o bin/sps-integer-ofwf -lpthread

bin/sps-integer-tiny: sps-integer.cpp BenchmarkSPS.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) sps-integer.cpp -o bin/sps-integer-tiny -lpthread $(TINYSTM_LIB)

bin/sps-integer-2plundo: sps-integer.cpp BenchmarkSPS.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) sps-integer.cpp -o bin/sps-integer-2plundo -lpthread

bin/sps-integer-2plundodist: sps-integer.cpp BenchmarkSPS.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) sps-integer.cpp -o bin/sps-integer-2plundodist -lpthread

bin/sps-integer-2plsf: sps-integer.cpp BenchmarkSPS.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) sps-integer.cpp -o bin/sps-integer-2plsf -lpthread

bin/sps-integer-tl2: sps-integer.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) sps-integer.cpp -o bin/sps-integer-tl2 -lpthread

bin/sps-integer-tlrweager: sps-integer.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) sps-integer.cpp -o bin/sps-integer-tlrweager -lpthread

bin/sps-integer-oreceager: sps-integer.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) sps-integer.cpp -o bin/sps-integer-oreceager -lpthread

bin/sps-integer-oreclazy: sps-integer.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) sps-integer.cpp -o bin/sps-integer-oreclazy -lpthread




#	
# Latency counters for volatile memory
#
bin/latency-counter-tl2lr: latency-counter.cpp BenchmarkLatencyCounter.hpp $(TL2LR_DEP)
	$(CXX) $(CXXFLAGS) -DUSE_TL2LR $(INCLUDES) ../stms/TL2LR.cpp latency-counter.cpp -o bin/latency-counter-tl2lr -lpthread

bin/latency-counter-tl2redo: latency-counter.cpp BenchmarkLatencyCounter.hpp ../stms/TL2Redo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_REDO $(INCLUDES) latency-counter.cpp -o bin/latency-counter-tl2redo -lpthread

bin/latency-counter-tl2orig: latency-counter.cpp BenchmarkLatencyCounter.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) latency-counter.cpp -o bin/latency-counter-tl2orig -lpthread

bin/latency-counter-ofwf: latency-counter.cpp BenchmarkLatencyCounter.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) latency-counter.cpp -o bin/latency-counter-ofwf -lpthread

bin/latency-counter-tiny: latency-counter.cpp BenchmarkLatencyCounter.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) latency-counter.cpp -o bin/latency-counter-tiny -lpthread $(TINYSTM_LIB)

bin/latency-counter-romlr: latency-counter.cpp BenchmarkLatencyCounter.hpp ../stms/romuluslr/RomulusLR.hpp ../stms/romuluslr/RomulusLR.cpp ../stms/romuluslr/malloc.cpp 
	$(CXX) $(CXXFLAGS) -DUSE_ROM_LR $(INCLUDES) ../stms/romuluslr/RomulusLR.cpp ../stms/romuluslr/malloc.cpp latency-counter.cpp -o bin/latency-counter-romlr -lpthread


#
# Sets Red-Black Tree 1M
#
bin/set-tree-1m-tl2lr: set-tree-1m.cpp BenchmarkSets.hpp $(TL2LR_DEP)
	$(CXX) $(CXXFLAGS) -DUSE_TL2LR $(INCLUDES) ../stms/TL2LR.cpp set-tree-1m.cpp -o bin/set-tree-1m-tl2lr -lpthread

bin/set-tree-1m-tl2redo: set-tree-1m.cpp BenchmarkSets.hpp ../stms/TL2Redo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_REDO $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-tl2redo -lpthread

bin/set-tree-1m-tl2orig: set-tree-1m.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-tree-1m.cpp -o bin/set-tree-1m-tl2orig -lpthread

bin/set-tree-1m-ofwf: set-tree-1m.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-ofwf -lpthread

bin/set-tree-1m-freedap: set-tree-1m.cpp BenchmarkSets.hpp ../stms/FreeDAP.hpp
	$(CXX) $(CXXFLAGS) -DUSE_FREEDAP $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-freedap -lpthread

bin/set-tree-1m-tiny: set-tree-1m.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-tree-1m.cpp -o bin/set-tree-1m-tiny -lpthread $(TINYSTM_LIB)

bin/set-tree-1m-romlr: set-tree-1m.cpp BenchmarkSets.hpp ../stms/romuluslr/RomulusLR.hpp ../stms/romuluslr/RomulusLR.cpp ../stms/romuluslr/malloc.cpp 
	$(CXX) $(CXXFLAGS) -DUSE_ROM_LR $(INCLUDES) ../stms/romuluslr/RomulusLR.cpp ../stms/romuluslr/malloc.cpp set-tree-1m.cpp -o bin/set-tree-1m-romlr -lpthread

bin/set-tree-1m-crwwp: set-tree-1m.cpp BenchmarkSets.hpp ../stms/CRWWPSTM.hpp
	$(CXX) $(CXXFLAGS) -DUSE_CRWWP $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-crwwp -lpthread

bin/set-tree-1m-2plundo: set-tree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-2plundo -lpthread

bin/set-tree-1m-2plundodist: set-tree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-2plundodist -lpthread

bin/set-tree-1m-2plsf: set-tree-1m.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-2plsf -lpthread

bin/set-tree-1m-tl2: set-tree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-tl2 -lpthread

bin/set-tree-1m-tlrweager: set-tree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-tlrweager -lpthread

bin/set-tree-1m-oreceager: set-tree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-oreceager -lpthread

bin/set-tree-1m-oreclazy: set-tree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-tree-1m.cpp -o bin/set-tree-1m-oreclazy -lpthread



#
# Sets B-Tree 1M
#
bin/set-btree-1m-tl2orig: set-btree-1m.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-btree-1m.cpp -o bin/set-btree-1m-tl2orig -lpthread

bin/set-btree-1m-2plundo: set-btree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-2plundo -lpthread

bin/set-btree-1m-2plundodist: set-btree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-2plundodist -lpthread

bin/set-btree-1m-2plsf: set-btree-1m.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-2plsf -lpthread

bin/set-btree-1m-tl2: set-btree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-tl2 -lpthread

bin/set-btree-1m-tlrweager: set-btree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-tlrweager -lpthread

bin/set-btree-1m-oreceager: set-btree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-oreceager -lpthread

bin/set-btree-1m-oreclazy: set-btree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-oreclazy -lpthread

bin/set-btree-1m-ofwf: set-btree-1m.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-btree-1m.cpp -o bin/set-btree-1m-ofwf -lpthread

bin/set-btree-1m-tiny: set-btree-1m.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-btree-1m.cpp -o bin/set-btree-1m-tiny -lpthread $(TINYSTM_LIB)


#
# Sets Relaxed AVL 1M
#
bin/set-ravl-1m-tl2orig: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-ravl-1m.cpp -o bin/set-ravl-1m-tl2orig -lpthread

bin/set-ravl-1m-2plundo: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-2plundo -lpthread

bin/set-ravl-1m-2plundodist: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-2plundodist -lpthread

bin/set-ravl-1m-2plsf: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-2plsf -lpthread

bin/set-ravl-1m-tl2: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-tl2 -lpthread

bin/set-ravl-1m-tlrweager: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-tlrweager -lpthread

bin/set-ravl-1m-oreceager: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-oreceager -lpthread

bin/set-ravl-1m-oreclazy: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-oreclazy -lpthread

bin/set-ravl-1m-ofwf: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-ravl-1m.cpp -o bin/set-ravl-1m-ofwf -lpthread

bin/set-ravl-1m-tiny: set-ravl-1m.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-ravl-1m.cpp -o bin/set-ravl-1m-tiny -lpthread $(TINYSTM_LIB)





#
# Sets Zip Tree 1M
#
bin/set-ziptree-1m-tl2orig: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-ziptree-1m.cpp -o bin/set-ziptree-1m-tl2orig -lpthread

bin/set-ziptree-1m-2plundo: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-2plundo -lpthread

bin/set-ziptree-1m-2plundodist: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-2plundodist -lpthread

bin/set-ziptree-1m-2plsf: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-2plsf -lpthread

bin/set-ziptree-1m-tl2: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-tl2 -lpthread

bin/set-ziptree-1m-tlrweager: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-tlrweager -lpthread

bin/set-ziptree-1m-oreceager: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-oreceager -lpthread

bin/set-ziptree-1m-oreclazy: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-oreclazy -lpthread

bin/set-ziptree-1m-tiny: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-ziptree-1m.cpp -o bin/set-ziptree-1m-tiny -lpthread $(TINYSTM_LIB)

bin/set-ziptree-1m-ofwf: set-ziptree-1m.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-ziptree-1m.cpp -o bin/set-ziptree-1m-ofwf -lpthread



#
# Sets SkipList 1M
#
bin/set-skiplist-1m-tl2orig: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-skiplist-1m.cpp -o bin/set-skiplist-1m-tl2orig -lpthread

bin/set-skiplist-1m-2plundo: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-2plundo -lpthread

bin/set-skiplist-1m-2plundodist: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-2plundodist -lpthread

bin/set-skiplist-1m-2plsf: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-2plsf -lpthread

bin/set-skiplist-1m-tl2: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-tl2 -lpthread

bin/set-skiplist-1m-tlrweager: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-tlrweager -lpthread

bin/set-skiplist-1m-oreceager: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-oreceager -lpthread

bin/set-skiplist-1m-oreclazy: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-oreclazy -lpthread

bin/set-skiplist-1m-ofwf: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-skiplist-1m.cpp -o bin/set-skiplist-1m-ofwf -lpthread

bin/set-skiplist-1m-tiny: set-skiplist-1m.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-skiplist-1m.cpp -o bin/set-skiplist-1m-tiny -lpthread $(TINYSTM_LIB)




#
# Linked List set  1k
#
bin/set-ll-1k-tl2orig: set-ll-1k.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-ll-1k.cpp -o bin/set-ll-1k-tl2orig -lpthread

bin/set-ll-1k-2plundo: set-ll-1k.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-2plundo -lpthread

bin/set-ll-1k-2plundodist: set-ll-1k.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-2plundodist -lpthread

bin/set-ll-1k-2plsf: set-ll-1k.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-2plsf -lpthread

bin/set-ll-1k-tl2: set-ll-1k.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-tl2 -lpthread

bin/set-ll-1k-tlrweager: set-ll-1k.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-tlrweager -lpthread

bin/set-ll-1k-oreceager: set-ll-1k.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-oreceager -lpthread

bin/set-ll-1k-oreclazy: set-ll-1k.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-oreclazy -lpthread

bin/set-ll-1k-tiny: set-ll-1k.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-ll-1k.cpp -o bin/set-ll-1k-tiny -lpthread $(TINYSTM_LIB)

bin/set-ll-1k-ofwf: set-ll-1k.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-ll-1k.cpp -o bin/set-ll-1k-ofwf -lpthread






#
# Partially Disjoint benchmark
#
bin/part-disjoint-tl2orig: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) part-disjoint.cpp -o bin/part-disjoint-tl2orig -lpthread

bin/part-disjoint-tiny: part-disjoint.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) part-disjoint.cpp -o bin/part-disjoint-tiny -lpthread $(TINYSTM_LIB)

bin/part-disjoint-2plundo: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-2plundo -lpthread

bin/part-disjoint-2plundodist: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-2plundodist -lpthread

bin/part-disjoint-2plsf: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-2plsf -lpthread

bin/part-disjoint-tl2: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-tl2 -lpthread

bin/part-disjoint-tlrweager: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-tlrweager -lpthread

bin/part-disjoint-oreceager: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-oreceager -lpthread

bin/part-disjoint-oreclazy: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-oreclazy -lpthread

bin/part-disjoint-ofwf: part-disjoint.cpp BenchmarkPartDisjoint.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) part-disjoint.cpp -o bin/part-disjoint-ofwf -lpthread


#
# Sets resizable hash 1M
#
bin/set-hash-10k-tl2orig: set-hash-10k.cpp BenchmarkSets.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) set-hash-10k.cpp -o bin/set-hash-10k-tl2orig -lpthread

bin/set-hash-10k-2plundo: set-hash-10k.cpp BenchmarkSets.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-2plundo -lpthread

bin/set-hash-10k-2plundodist: set-hash-10k.cpp BenchmarkSets.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-2plundodist -lpthread

bin/set-hash-10k-2plsf: set-hash-10k.cpp BenchmarkSets.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-2plsf -lpthread

bin/set-hash-10k-tl2: set-hash-10k.cpp BenchmarkSets.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-tl2 -lpthread

bin/set-hash-10k-tlrweager: set-hash-10k.cpp BenchmarkSets.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-tlrweager -lpthread

bin/set-hash-10k-oreceager: set-hash-10k.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-oreceager -lpthread

bin/set-hash-10k-oreclazy: set-hash-10k.cpp BenchmarkSets.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-oreclazy -lpthread

bin/set-hash-10k-tiny: set-hash-10k.cpp BenchmarkSets.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-hash-10k.cpp -o bin/set-hash-10k-tiny -lpthread $(TINYSTM_LIB)

bin/set-hash-10k-ofwf: set-hash-10k.cpp BenchmarkSets.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) set-hash-10k.cpp -o bin/set-hash-10k-ofwf -lpthread



#
# Maps Relaxed AVL
#
bin/map-ravl-tl2orig: map-ravl.cpp BenchmarkMaps.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) map-ravl.cpp -o bin/map-ravl-tl2orig -lpthread

bin/map-ravl-2plundo: map-ravl.cpp BenchmarkMaps.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) map-ravl.cpp -o bin/map-ravl-2plundo -lpthread

bin/map-ravl-2plundodist: map-ravl.cpp BenchmarkMaps.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) map-ravl.cpp -o bin/map-ravl-2plundodist -lpthread

bin/map-ravl-2plsf: map-ravl.cpp BenchmarkMaps.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) map-ravl.cpp -o bin/map-ravl-2plsf -lpthread

bin/map-ravl-tl2: map-ravl.cpp BenchmarkMaps.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) map-ravl.cpp -o bin/map-ravl-tl2 -lpthread

bin/map-ravl-tlrweager: map-ravl.cpp BenchmarkMaps.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) map-ravl.cpp -o bin/map-ravl-tlrweager -lpthread

bin/map-ravl-oreceager: map-ravl.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) map-ravl.cpp -o bin/map-ravl-oreceager -lpthread

bin/map-ravl-oreclazy: map-ravl.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) map-ravl.cpp -o bin/map-ravl-oreclazy -lpthread

bin/map-ravl-tiny: map-ravl.cpp BenchmarkMaps.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) map-ravl.cpp -o bin/map-ravl-tiny -lpthread $(TINYSTM_LIB)

bin/map-ravl-ofwf: map-ravl.cpp BenchmarkMaps.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) map-ravl.cpp -o bin/map-ravl-ofwf -lpthread


	
#
# Maps Skiplist 1m
#
bin/map-skiplist-tl2orig: map-skiplist.cpp BenchmarkMaps.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) map-skiplist.cpp -o bin/map-skiplist-tl2orig -lpthread

bin/map-skiplist-2plundo: map-skiplist.cpp BenchmarkMaps.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-2plundo -lpthread

bin/map-skiplist-2plundodist: map-skiplist.cpp BenchmarkMaps.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-2plundodist -lpthread

bin/map-skiplist-2plsf: map-skiplist.cpp BenchmarkMaps.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-2plsf -lpthread

bin/map-skiplist-tl2: map-skiplist.cpp BenchmarkMaps.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-tl2 -lpthread

bin/map-skiplist-tlrweager: map-skiplist.cpp BenchmarkMaps.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-tlrweager -lpthread

bin/map-skiplist-oreceager: map-skiplist.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-oreceager -lpthread

bin/map-skiplist-oreclazy: map-skiplist.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-oreclazy -lpthread

bin/map-skiplist-tiny: map-skiplist.cpp BenchmarkMaps.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) map-skiplist.cpp -o bin/map-skiplist-tiny -lpthread $(TINYSTM_LIB)

bin/map-skiplist-ofwf: map-skiplist.cpp BenchmarkMaps.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) map-skiplist.cpp -o bin/map-skiplist-ofwf -lpthread



#
# Maps ZipTree 1m
#
bin/map-ziptree-tl2orig: map-ziptree.cpp BenchmarkMaps.hpp ../stms/TL2Orig.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2_ORIG $(INCLUDES) $(TL2_CSRC) map-ziptree.cpp -o bin/map-ziptree-tl2orig -lpthread

bin/map-ziptree-2plundo: map-ziptree.cpp BenchmarkMaps.hpp ../stms/2PLUndo.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-2plundo -lpthread

bin/map-ziptree-2plundodist: map-ziptree.cpp BenchmarkMaps.hpp ../stms/2PLUndoDist.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PL_UNDO_DIST $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-2plundodist -lpthread

bin/map-ziptree-2plsf: map-ziptree.cpp BenchmarkMaps.hpp ../stms/2PLSF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_2PLSF $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-2plsf -lpthread

bin/map-ziptree-tl2: map-ziptree.cpp BenchmarkMaps.hpp ../stms/zardoshti/tl2_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TL2 $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-tl2 -lpthread

bin/map-ziptree-tlrweager: map-ziptree.cpp BenchmarkMaps.hpp ../stms/zardoshti/tlrw_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TLRW_EAGER $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-tlrweager -lpthread

bin/map-ziptree-oreceager: map-ziptree.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_eager_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_EAGER $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-oreceager -lpthread

bin/map-ziptree-oreclazy: map-ziptree.cpp BenchmarkMaps.hpp ../stms/zardoshti/orec_lazy_wrap.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OREC_LAZY $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-oreclazy -lpthread

bin/map-ziptree-tiny: map-ziptree.cpp BenchmarkMaps.hpp ../stms/TinySTM.hpp ../stms/tinystm/lib/libstm.a
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) map-ziptree.cpp -o bin/map-ziptree-tiny -lpthread $(TINYSTM_LIB)

bin/map-ziptree-ofwf: map-ziptree.cpp BenchmarkMaps.hpp ../stms/OneFileWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) map-ziptree.cpp -o bin/map-ziptree-ofwf -lpthread



	
	
