#!/bin/bash
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

VICTIM=$1;

HOST=$kserial

let IPPORT=$TW_BASE_PORT+2
let ENVPORT=$TW_BASE_PORT+4
echo simip $HOST:$IPPORT $HOST:$ENVPORT
exec simip $HOST:$IPPORT $HOST:$ENVPORT
