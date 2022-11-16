set term postscript color eps enhanced 22
set output "latency-cervino.eps"

set size 0.95,0.6

X=0.04
W=0.26
M=0.03

load "styles.inc"

set tmargin 11.8
set bmargin 2.5

set multiplot layout 1,3

unset key

set grid ytics

set xtics ("" 1, "" 4, 8, 16, 24, 32) nomirror out offset -0.25,0.5
set label at screen 0.5,0.03 center "Number of threads"
set label at screen 0.5,0.57 center "Throughput and latency percentiles for pair-wise conflicts"

set xrange [1:32]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ytics 2 offset 0.5,0 font "Helvetica Condensed"
set yrange [0:14]
set grid mytics
#set style textbox opaque noborder fillcolor rgb "white"
#set label at first 1,14 front boxed left offset -0.5,0 "14"
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "Throughput (({/Symbol \264}1000 txn/s)"

plot \
    '../data/cervino/part-disjoint-tl2orig.txt'    using 1:($2/1e3) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-tiny.txt'       using 1:($2/1e3) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-oreceager.txt'  using 1:($2/1e3) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-ofwf.txt'       using 1:($2/1e3) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/part-disjoint-2plsf.txt'      using 1:($2/1e3) with linespoints notitle ls 10 lw 3 dt 1


set mytics 1
set grid nomytics

unset ylabel
set ytics format ""

set lmargin at screen X+(W+1.8*M)
set rmargin at screen X+(W+1.8*M)+W

unset label

#set ylabel offset 1.0,0 "miliseconds"
set ytics 10 offset 0.5,0
set format y "%g"
set yrange [0:80]

set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "P90 (milliseconds)"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/cervino/latency-tl2orig.txt'    using 1:($2/1e3) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/latency-tiny.txt'       using 1:($2/1e3) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/latency-oreceager.txt'  using 1:($2/1e3) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/latency-ofwf.txt'       using 1:($2/1e3) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/latency-2plsf.txt'      using 1:($2/1e3) with linespoints notitle ls 10 lw 3 dt 1

set lmargin at screen X+2*(W+1.8*M)
set rmargin at screen X+2*(W+1.8*M)+W

unset label
set ytics 10 offset 0.5,0
set format y "%g"
set yrange [0:80]
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" " P99 (milliseconds)"

plot \
    '../data/cervino/latency-tl2orig.txt'    using 1:($3/1e3) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/latency-tiny.txt'       using 1:($3/1e3) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/latency-oreceager.txt'  using 1:($3/1e3) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/latency-ofwf.txt'       using 1:($3/1e3) with linespoints notitle ls 7  lw 3 dt (1,1), \
    '../data/cervino/latency-2plsf.txt'      using 1:($3/1e3) with linespoints notitle ls 10 lw 3 dt 1




unset tics
unset border
unset xlabel
unset ylabel
unset label

set object 1 rect from screen 0.06,0.42 to screen 0.21,0.44
set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder
set object 2 rect from screen 0.06,0.37 to screen 0.21,0.39
set object 2 rect fc rgb 'white' fillstyle solid 1.0 noborder

set key at screen 0.22,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
plot [][0:1] \
    2 with linespoints title 'TL2'           ls 1, \
    2 with linespoints title 'TinySTM'       ls 3, \
    2 with linespoints title 'OREC-Z'        ls 5

set object 1 rect from screen 0.46,0.42 to screen 0.59,0.44
set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder
unset object 2

set key at screen 0.60,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
plot [][0:1] \
    2 with linespoints title 'OFWF'          ls 7, \
    2 with linespoints title '2PLSF'         ls 10

unset multiplot
    