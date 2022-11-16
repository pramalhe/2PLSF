set term postscript color eps enhanced 22
set output "sps-integer-cervino.eps"

set size 0.95,0.6

X=0.1
W=0.26
M=0.025

load "styles.inc"

set tmargin 11.8
set bmargin 2.5

set multiplot layout 1,3

unset key

set grid ytics

set xtics ("" 1, "" 4, "" 8, 16, 32, 48, 64) nomirror out offset -0.25,0.5
set label at screen 0.5,0.03 center "Number of threads"
set label at screen 0.5,0.57 center "Swap Integers Benchmark with array of 10^{7} integers"

set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.0,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 1 offset 0.5,0
set format y "%g"
set yrange [0:20]
set grid mytics

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "2 swaps/txn"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/cervino/sps-integer-tl2orig.txt'       using 1:($2/(1e6*2)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tiny.txt'          using 1:($2/(1e6*2)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tlrweager.txt'     using 1:($2/(1e6*2)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-oreceager.txt'     using 1:($2/(1e6*2)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-ofwf.txt'          using 1:($2/(1e6*2)) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-2plundodistsf.txt' using 1:($2/(1e6*2)) with linespoints notitle ls 10 lw 3 dt 1

set mytics 1
set grid nomytics

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label

set ytics 1 offset 0.5,0
set yrange [0:1]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,25 front boxed left offset -0.5,0 "25"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "32 swaps/txn"

plot \
    '../data/cervino/sps-integer-tl2orig.txt'       using 1:($3/(1e6*32)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tiny.txt'          using 1:($3/(1e6*32)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tlrweager.txt'     using 1:($3/(1e6*32)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-oreceager.txt'     using 1:($3/(1e6*32)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-ofwf.txt'          using 1:($3/(1e6*32)) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-2plundodistsf.txt' using 1:($3/(1e6*32)) with linespoints notitle ls 10 lw 3 dt 1


set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 1 offset 0.5,0
set yrange [0:4]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,60 front boxed left offset -0.5,0 "60"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "128 swaps/txn"

plot \
    '../data/cervino/sps-integer-tl2orig.txt'       using 1:($4/(1e6*128)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tiny.txt'          using 1:($4/(1e6*128)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-tlrweager.txt'     using 1:($4/(1e6*128)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-oreceager.txt'     using 1:($4/(1e6*128)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-ofwf.txt'          using 1:($4/(1e6*128)) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/sps-integer-2plundodistsf.txt' using 1:($4/(1e6*128)) with linespoints notitle ls 10 lw 3 dt 1



unset tics
unset border
unset xlabel
unset ylabel
unset label
set key at screen 0.79,0.48 samplen 2.0 font ",10" noinvert width 1
plot [][0:1] \
    2 with linespoints title 'TL2'           ls 1, \
    2 with linespoints title 'TinySTM'       ls 3, \
    2 with linespoints title 'TLRW-Z'        ls 4, \
    2 with linespoints title 'OREC-Z'        ls 5, \
    2 with linespoints title 'OFWF'          ls 7, \
    2 with linespoints title '2PLSF'         ls 10
#    2 with linespoints title 'TL2-Undo'      ls 2, \
    2 with linespoints title '2PL-U-Dist'    ls 9, \
    2 with linespoints title 'TL2-Z'         ls 3, \
    2 with linespoints title 'orec-lazy-Z'   ls 6, \
    2 with linespoints title '2PL-U'         ls 8, \
	
unset multiplot
    