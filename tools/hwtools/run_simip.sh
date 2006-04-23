#!/bin/bash
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

source ${0%/*}/kconf_lib
set -e

VICTIM=$1
if [ -n "$VICTIM" ]; then
    HW_VICTIM=$VICTIM
fi

kconf_flatten_export mambo


HOST=$(kconf_get $HW_VICTIM kserial)

if [ -z "$TW_BASE_PORT" -o "$TW_BASE_PORT" = "#" ]; then
	bp=$(kconf_get $HW_VICTIM TW_BASE_PORT)
	if [ "$bp" = "#" ] ; then
	    bp=$(kconf_get $HW_VICTIM MAMBO_SIMULATOR_PORT);
	fi
	if [ "$bp" = "#" ] ; then
	    bp=$MAMBO_SIMULATOR_PORT;
	fi
else
	let bp=$TW_BASE_PORT
fi

let IPPORT=$bp+2
let ENVPORT=$bp+4
echo simip $HOST:$IPPORT $HOST:$ENVPORT
exec simip $HOST:$IPPORT $HOST:$ENVPORT
