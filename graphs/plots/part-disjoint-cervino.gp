set term postscript color eps enhanced 22
set output "part-disjoint-cervino.eps"

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

set xtics ("" 1, "" 2, "" 4, 8, 16, 32, 48, 64) nomirror out offset -0.25,0.5
set label at screen 0.5,0.03 center "Number of threads"
set label at screen 0.5,0.57 center "Partially Disjoint workload"

set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.0,0 "Operations ({/Symbol \264}10^1/s)"
set ytics 20 offset 0.5,0
set format y "%g"
set yrange [0:200]
set grid mytics

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Reverse order"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/cervino/part-disjoint-tl2orig.txt'       using 1:($2/1e1) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-tl2undo.txt'       using 1:($2/1e1) with linespoints notitle ls 2  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-tl2.txt'           using 1:($2/1e1) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-tlrweager.txt'     using 1:($2/1e1) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-ofwf.txt'          using 1:($2/1e1) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-2plundo.txt'       using 1:($2/1e1) with linespoints notitle ls 6  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-2plundodist.txt'   using 1:($2/1e1) with linespoints notitle ls 7  lw 3 dt 1, \
    '../data/cervino/part-disjoint-2plsf.txt'         using 1:($2/1e1) with linespoints notitle ls 8  lw 3 dt 1

set mytics 1
set grid nomytics

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label

set ytics 2 offset 0.5,0
set yrange [0:18]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,18 front boxed left offset -0.5,0 "18"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "BOGUS"

plot \
    '../data/cervino/set-ravl-1m-tl2orig.txt'       using 1:($3/1e6) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-tl2undo.txt'       using 1:($3/1e6) with linespoints notitle ls 2  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-dztl2sf.txt'       using 1:($3/1e6) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-2plundo.txt'       using 1:($3/1e6) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-2plundodist.txt'   using 1:($3/1e6) with linespoints notitle ls 4  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plsf.txt'         using 1:($3/1e6) with linespoints notitle ls 5  lw 3 dt 1


set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 2 offset 0.5,0
set yrange [0:20]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,20 front boxed left offset -0.5,0 "20"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "BOGUS"

plot \
    '../data/cervino/set-ravl-1m-tl2orig.txt'       using 1:($5/1e6) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-tl2undo.txt'       using 1:($5/1e6) with linespoints notitle ls 2  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-2plundo.txt'       using 1:($5/1e6) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/set-ravl-1m-2plundodist.txt'   using 1:($5/1e6) with linespoints notitle ls 4  lw 3 dt 1, \
    '../data/cervino/set-ravl-1m-2plsf.txt'         using 1:($5/1e6) with linespoints notitle ls 5  lw 3 dt 1



unset tics
unset border
unset xlabel
unset ylabel
unset label
set key at screen 0.80,0.48 samplen 2.0 font ",10" noinvert width 1
plot [][0:1] \
    2 with linespoints title 'TL2-Orig'      ls 1, \
    2 with linespoints title 'TL2-Undo'      ls 2, \
    2 with linespoints title 'TL2-Z'         ls 3, \
    2 with linespoints title 'TLRW-Z'        ls 4, \
    2 with linespoints title 'OFWF'          ls 5, \
    2 with linespoints title '2PL-U'         ls 6, \
    2 with linespoints title '2PL-U-Dist'    ls 7, \
    2 with linespoints title '2PL-U-SF'      ls 8
	
unset multiplot
    