#!/usr/bin/env bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: runtraceProfile.sh,v 1.1 2004/02/10 18:40:35 aabauman Exp $
# ############################################################################

# this script runs traceProfile and tracePostProc, guessing at tracePostProc
# arguments from the traceProfile ones

PPARGS=""

for arg in $*; do
    if [ "$arg" == "--kmon" ]; then
    	RUNKMON=1
    fi

    if [ "$lastarg" == "--path" ]; then
        PPARGS="$PPARGS --path $arg"
    fi
    lastarg=$arg
done

echo "traceProfile $* | tracePostProc --stdin $PPARGS"
traceProfile $* | tracePostProc --stdin $PPARGS

[ $? -eq 0 ] || exit $?

if [ $RUNKMON ]; then
    echo "tracePostProc --kmon $PPARGS"
    tracePostProc --kmon $PPARGS
fi
