#!/bin/sh

for i in \
set-ravl-1m-2pl.gp \
set-ll-1k-cervino.gp \
set-hash-10k-cervino.gp \
set-skiplist-1m-cervino.gp \
set-ziptree-1m-cervino.gp \
set-ravl-1m-cervino.gp \
maps-98u-100k.gp \
latency-cervino.gp \
ycsb-cervino.gp \
;
do
  echo "Processing:" $i
  gnuplot $i
  epstopdf `basename $i .gp`.eps
  rm `basename $i .gp`.eps
done

# set-btree-1m-cervino.gp \
# set-hash-10k-cervino.gp \
# set-ravl-1m-cervino.gp \
# set-skiplist-1m-cervino.gp \
# set-tree-1m-cervino.gp \
# set-ziptree-1m-cervino.gp \
# set-ll-1k-cervino.gp \
# latency-cervino.gp \
# ycsb-cervino.gp \
# sps-integer-cervino.gp \
# set-ravl-1m-2pl.gp \
# maps-98u-100k.gp \
# maps-100u-100k.gp \
# maps-98u-100k-c6a.48xlarge.gp \
# set-ravl-1m-c6a.48xlarge.gp \
