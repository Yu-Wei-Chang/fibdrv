reset
set term png enhanced font 'Verdana,10'
set output 'time_cost_fd_comp.png'
set title 'Calculation time of Fibonacci sequence - Fast doubling'
set xlabel "Fibonacci sequence number"
set ylabel "Time cost(ns)"
set grid

plot [0:100][0:10000] 'kernel_cost.txt' using 1:2 with linespoints linewidth 2 title 'Original approach', \
'kernel_cost.txt' using 1:3 with linespoints linewidth 2 title 'Fast-doubling approach'
