#!/bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: ltp.sh,v 1.2 2005/06/29 19:14:07 apw Exp $
# ############################################################################

set -e

ltp=/usr/share/ltp/ltp-full-20050608;

test -d $ltp || {
    echo "ltp: skipping LTP tests since none found";
    exit 0;
}

cd $ltp/testcases/kernel/syscalls/nanosleep
./nanosleep01

cd $ltp/testcases/kernel/syscalls/getrusage
./getrusage01

cd $ltp/testcases/kernel/syscalls/alarm
./alarm01
./alarm02
./alarm03
./alarm04
./alarm05
./alarm06
./alarm07
