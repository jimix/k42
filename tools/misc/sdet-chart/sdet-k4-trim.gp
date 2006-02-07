# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# This is a gnuplot script which generates a chart representing only
# more recent runs of nightly SDET.
#
#  $Id: sdet-k4-trim.gp,v 1.1 2005/04/14 21:08:25 apw Exp $
# ############################################################################

set terminal png
set output "sdet-k4-trim.png"
set xlabel 'Date'
set ylabel 'SDET Performance'
set title 'SDET Performance on IBM 270'

set xdata time
set timefmt "%Y-%m-%d"
set format x "%b %Y"
set xtics 6900000
set yrange [:3700]
set xrange ["2004-03-06":]
set pointsize 0.2

plot 'sdet4_results-noDeb.dat' using 1:2   \
 title '4-way SDET cold' with points,      \
  'sdet4_results-noDeb.dat' using 1:4      \
    title '4-way SDET hot' with points,    \
     'sdet1_results-noDeb.dat' using 1:2   \
      title '1-way SDET cold' with points, \
       'sdet1_results-noDeb.dat' using 1:4 \
        title '1-way SDET hot' with points
