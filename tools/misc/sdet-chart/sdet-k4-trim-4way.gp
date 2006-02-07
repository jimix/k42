# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# This is a gnuplot script which generates a chart representing only
# more recent runs of nightly SDET, restricting itself to the 4-way
# data so trends can be better seen.
#
#  $Id: sdet-k4-trim-4way.gp,v 1.8 2005/08/26 16:33:36 apw Exp $
# ############################################################################

set terminal png
set output "sdet-k4-trim-4way.png"
set xlabel 'Date'
set ylabel 'SDET Performance'
set title "Historical SDET Performance on IBM 270 (Generated on `date`)"
set key left

set xdata time
set timefmt "%Y-%m-%d"
set format x "%b %Y"
set xtics rotate
set yrange [2700:3500]
set xrange ["2004-03-06":]
set pointsize 0.17

plot 'sdet4_results-noDeb.dat' using 1:2     \
 title '4-way SDET minimum' with points,     \
  'sdet4_results-noDeb.dat' using 1:3        \
    title '4-way SDET average' with points,  \
     'sdet4_results-noDeb.dat' using 1:4     \
      title '4-way SDET maximum' with linespoints
