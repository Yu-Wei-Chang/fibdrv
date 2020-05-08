reset
set term png enhanced font 'Verdana,10'
set output 'time_cost.png'
set title 'Calculation time of Fibonacci sequence'
set xlabel "Fibonacci sequence number"
set ylabel "Time cost(ns)"
set grid

plot [0:100][0:3000] 'user_cost.txt' using 1:2 with linespoints linewidth 2 title 'User', \
'' using 1:3 with linespoints linewidth 2 title 'Overhead between user <-> kernel', \
'kernel_cost.txt' using 1:2 with linespoints linewidth 2 title 'Kernel'
