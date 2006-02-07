#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# This program expects one argument, which is the name of the raw data
# file generated during a nightly SDET performance run.  It outputs on
# stdout a cleaned up version of the same data, eliding rows with empty
# values and tranforming the dates into ISO format for later processing.
#
#  $Id: clean.sh,v 1.1 2005/04/14 21:04:35 apw Exp $
# ############################################################################

egrep '^20..-.*:[0-9][0-9] [0-9]{3,} [0-9]{3,} [0-9]{3,}' $1 | 
  while read day col1 col2 col3; do
    printf "%s %s %s %s\n" \
      $(echo $day | sed 's,-, ,g' | 
        awk '{printf "%s %s %s\n", $3, $2, $1}' | 
	  date -I -f -) $col1 $col2 $col3; done
