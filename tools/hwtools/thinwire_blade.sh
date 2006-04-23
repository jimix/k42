#!/bin/bash
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
VICTIM=$1

source ${0%/*}/kconf_lib
set -e

bp=`kconf_get $VICTIM TW_BASE_PORT`
ktw=`kconf_get $VICTIM ktw`

exec thinwire3 ${ktw} stdin $[$bp + 1] $[$bp + 2] $[$bp + 3] \
			    : $[$bp +8] $[$bp +9] \
			    : $[$bp +16] $[$bp +17]
