set term postscript color eps enhanced 22
set output "ycsb-cervino.eps"

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
set label at screen 0.5,0.57 center "YCSB with 10^{7} keys"

set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.2,0 "Transactions ({/Symbol \264}10^6/s)"
set ytics 0.5 offset 0.5,0 font "Helvetica Condensed"
set format y "%g"
set yrange [0:1.1]
set grid mytics

set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "High 50%w 50%r"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/cervino/ycsb-high-tictoc.txt'       using 1:($1*$2/($3*1e6)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/ycsb-high-no_wait.txt'      using 1:($1*$2/($3*1e6)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/ycsb-high-dl_detect.txt'    using 1:($1*$2/($3*1e6)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/ycsb-high-wait_die.txt'     using 1:($1*$2/($3*1e6)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/ycsb-high-2plsf.txt'        using 1:($1*$2/($3*1e6)) with linespoints notitle ls 10 lw 3 dt 1

set mytics 1
set grid nomytics

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label

set ytics 1 offset 0.5,0
set yrange [0:3]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,3 front boxed left offset -0.5,0 "3" font "Helvetica Condensed"
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "Medium 10%w 90%r"

plot \
    '../data/cervino/ycsb-med-tictoc.txt'       using 1:($1*$2/($3*1e6)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/ycsb-med-no_wait.txt'      using 1:($1*$2/($3*1e6)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/ycsb-med-dl_detect.txt'    using 1:($1*$2/($3*1e6)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/ycsb-med-wait_die.txt'     using 1:($1*$2/($3*1e6)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/ycsb-med-2plsf.txt'        using 1:($1*$2/($3*1e6)) with linespoints notitle ls 10 lw 3 dt 1


set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 1 offset 0.5,0
set yrange [0:20]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,20 front boxed left offset -0.5,0 "20" font "Helvetica Condensed"
set label at graph 0.5,1.075 center font "Helvetica-bold Condensed,18" "Low 0%w 100%r"

plot \
    '../data/cervino/ycsb-low-tictoc.txt'       using 1:($1*$2/($3*1e6)) with linespoints notitle ls 1  lw 3 dt (1,1), \
    '../data/cervino/ycsb-low-no_wait.txt'      using 1:($1*$2/($3*1e6)) with linespoints notitle ls 3  lw 3 dt (1,1), \
    '../data/cervino/ycsb-low-dl_detect.txt'    using 1:($1*$2/($3*1e6)) with linespoints notitle ls 4  lw 3 dt (1,1), \
    '../data/cervino/ycsb-low-wait_die.txt'     using 1:($1*$2/($3*1e6)) with linespoints notitle ls 5  lw 3 dt (1,1), \
    '../data/cervino/ycsb-low-2plsf.txt'        using 1:($1*$2/($3*1e6)) with linespoints notitle ls 10 lw 3 dt 1



unset tics
unset border
unset xlabel
unset ylabel
unset label
set key at screen 0.28,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1

set object 1 rect from screen 0.11,0.43 to screen 0.27,0.45
set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder

plot [][0:1] \
    2 with linespoints title 'TICTOC'       ls 1, \
    2 with linespoints title 'NO\_WAIT'     ls 3

set object 1 rect from screen 0.395,0.415 to screen 0.575,0.435
set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder

set key at screen 0.585,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
plot [][0:1] \
    2 with linespoints title 'DL\_DETECT'   ls 4, \
    2 with linespoints title 'WAIT\_DIE'    ls 5

unset object 1

set key at screen 0.85,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
plot [][0:1] \
    2 with linespoints title '2PLSF'        ls 10



# set object 1 rect from screen 0.06,0.42 to screen 0.21,0.44
# set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder
# set object 2 rect from screen 0.06,0.37 to screen 0.21,0.39
# set object 2 rect fc rgb 'white' fillstyle solid 1.0 noborder

# set key at screen 0.22,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
# plot [][0:1] \
#     2 with linespoints title 'TL2'           ls 1, \
#     2 with linespoints title 'TinySTM'       ls 3, \
#     2 with linespoints title 'OREC-Z'        ls 5

# set object 1 rect from screen 0.46,0.42 to screen 0.59,0.44
# set object 1 rect fc rgb 'white' fillstyle solid 1.0 noborder
# unset object 2

# set key at screen 0.60,0.48 samplen 2.0 font "Helvetica Condensed,18" noinvert width 1
# plot [][0:1] \
#     2 with linespoints title 'OFWF'          ls 7, \
#     2 with linespoints title '2PLSF'         ls 10

unset multiplot

unset multiplot
    
