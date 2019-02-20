#! /usr/local/cs/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. average wait per lock time (ns)
#
# output:
#	lab2b_1.png ... total number of operations per second vs number of threads
#	lab2b_2.png ... wait time and cost per operation vs number of threads
#	lab2b_3.png ... threads and iterations that run (protected) w/o failure
#	lab2b_4.png ... total number of operations per second vs number of threads
#	lab2b_5.png ... total number of operations per second vs number of threads
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

set title "List-1: Scalability of synchronization mechanisms accounting parallelism"
set xlabel "threads"
set logscale x 2
set ylabel "Total operations per second(ns)"
set logscale y
set output 'lab2b_1.png'

plot \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'mutex' with linespoints lc rgb 'blue', \
     "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'spin-lock' with linespoints lc rgb 'green'

set title "List-2: Wait time and cost per operation with mutex"
set xlabel "Threads"
set logscale x 2
set ylabel "Cost per operation / Wait time"
set logscale y 10
set output 'lab2b_2.png'
# note that unsuccessful runs should have produced no output
plot \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
	title 'wait-for-lock' with linespoints lc rgb 'blue', \
     "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
	title 'cost' with linespoints lc rgb 'green'

set title "List-3: Unprotected Threads and Iterations that run without failure with four lists"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:20]
set ylabel "Successful iterations"
set logscale y 10
set output 'lab2b_3.png'
plot \
     "< grep 'list-id-none' lab2b_list.csv" using ($2):($3) \
     	title 'w/o protection' with points lc rgb 'blue', \
     "< grep 'list-id-m' lab2b_list.csv" using ($2):($3) \
     	title 'sync=m' with points lc rgb 'green', \
     "< grep 'list-id-s' lab2b_list.csv" using ($2):($3) \
     	title 'sync=s' with points lc rgb 'red', \

set title "List-4: Scalability of mutex accounting parallelism and multiple lists"
set xlabel "threads"
set logscale x 2
unset xrange
set ylabel "Total operations per second(ns)"
set logscale y
set output 'lab2b_4.png'

plot \
     "< grep -E 'list-none-m,[0-9],1000,1,|list-none-m,12,1000,1,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=1' with linespoints lc rgb 'blue', \
     "< grep 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< grep 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=8' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=16' with linespoints lc rgb 'orange'

set title "List-5: Scalability of spin-lock accounting parallelism and multiple lists"
set xlabel "threads"
set logscale x 2
set ylabel "Total operations per second(ns)"
set logscale y
set output 'lab2b_5.png'

plot \
     "< grep -E 'list-none-s,[0-9],1000,1,|list-none-s,12,1000,1,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=1' with linespoints lc rgb 'blue', \
     "< grep 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=4' with linespoints lc rgb 'green', \
     "< grep 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=8' with linespoints lc rgb 'red', \
     "< grep 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000)/($7) \
	title 'lists=16' with linespoints lc rgb 'orange'