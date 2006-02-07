# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# This is a gnuplot script which generates a chart representing every
# valid run of nightly SDET.  There is such a difference in scale between
# the first recorded runs and the more recent runs that it is hard to see
# any trends.
#
#  $Id: sdet-k4-all.gp,v 1.1 2005/04/14 21:06:21 apw Exp $
# ############################################################################

set terminal png
set output "sdet-k4-all.png"
set xlabel 'Date'
set ylabel 'SDET Performance'
set title 'SDET Performance on 4-way IBM 270'

set xdata time
set timefmt "%Y-%m-%d"
set format x "%b %Y"
set xtics 16900000
set pointsize 0.2

plot 'sdet1_results-noDeb.dat' using 1:2 \
 title '1-way SDET cold' with points, \
  'sdet1_results-noDeb.dat' using 1:4 \
   title '1-way SDET hot' with points, \
    'sdet4_results-noDeb.dat' using 1:2 \
     title '4-way SDET cold' with points, \
      'sdet4_results-noDeb.dat' using 1:4 \
       title '4-way SDET hot' with points
