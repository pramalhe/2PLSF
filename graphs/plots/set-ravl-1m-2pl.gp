set term postscript color eps enhanced 22
set output "set-ravl-1m-2pl.eps"

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
set label at screen 0.5,0.57 center "Relaxed AVL tree with 10^{6} keys"

set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.0,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 5 offset 0.5,0 font "Helvetica Condensed"
set format y "%g"
set yrange [0:15]
set grid mytics

set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "i=50% r=50% l=0%"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/cervino/set-ravl-1m-2plundo.txt'       using 1:($2/1e6) with linespoints notitle ls 8  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plundodist.txt'   using 1:($2/1e6) with linespoints notitle ls 9  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plsf.txt'         using 1:($2/1e6) with linespoints notitle ls 10 lw 3 dt 1

set mytics 1
set grid nomytics

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label

set ytics 5 offset 0.5,0
set yrange [0:25]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,25 front boxed left offset -0.5,0 "25" font "Helvetica Condensed"
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "i=10% r=10% l=80%"

plot \
    '../data/cervino/set-ravl-1m-2plundo.txt'       using 1:($3/1e6) with linespoints notitle ls 8  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plundodist.txt'   using 1:($3/1e6) with linespoints notitle ls 9  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plsf.txt'         using 1:($3/1e6) with linespoints notitle ls 10 lw 3 dt 1


set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 10 offset 0.5,0
set yrange [0:50]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,50 front boxed left offset -0.5,0 "50" font "Helvetica Condensed"
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "i=0% r=0% l=100%"

plot \
    '../data/cervino/set-ravl-1m-2plundo.txt'       using 1:($4/1e6) with linespoints notitle ls 8  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plundodist.txt'   using 1:($4/1e6) with linespoints notitle ls 9  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plsf.txt'         using 1:($4/1e6) with linespoints notitle ls 10 lw 3 dt 1



unset tics
unset border
unset xlabel
unset ylabel
unset label
set object 1 rect from screen 0.68,0.40 to screen 0.87,0.42
set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder
set key at screen 0.87,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
plot [][0:1] \
    2 with linespoints title '2PLSF'         ls 10, \
    2 with linespoints title '2PL-RW-Dist'   ls 9, \
    2 with linespoints title '2PL-RW'        ls 8
	
unset multiplot
    