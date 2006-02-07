#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: lpar_prep.sh,v 1.2 2005/03/03 22:53:46 mostrows Exp $
# ############################################################################

FILE=$1
LPAR=$2

if [ -z "$FILE" -o -z "$LPAR" ] ; then
    echo "Usage: lpar_prep <file> <lpar_name>";
    exit 1;
fi

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

exec hype_prep_lpar -C -n $LPAR -i $FILE 
