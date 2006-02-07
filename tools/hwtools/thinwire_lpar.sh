#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: thinwire_lpar.sh,v 1.2 2005/03/03 22:52:57 mostrows Exp $
# ############################################################################

LPAR=$1

if [ -z "$LPAR" ] ; then
    echo "Usage: thinwire_lpar <lpar_name> <ignored....>";
    exit 1;
fi

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

base=$[$TW_BASE_PORT-1]




exec thinwire3 localhost:$base stdin `seq $[$base+2] $[$base+5]`  

