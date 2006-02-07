#!/bin/bash
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

# Usage: thinwire_mambo <victim> <breaklocks> <lock message>

VICTIM=$1

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

if [ -z "$TW_BASE_PORT" ] ; then
    exit -1;
fi

bp=$TW_BASE_PORT

target=localhost:$MAMBO_SIMULATOR_PORT

if [ $HW_VERBOSE -ge 4 ] ; then
    debug="-debug -verbose"
fi

exec thinwire3 ${debug} ${target} \
    stdout $[$bp + 1] $[$bp + 2] $[$bp + 3] $[$bp + 4] \
    : $[$bp + 8] $[$bp + 9] : $[$bp + 16] $[$bp + 17] : 
