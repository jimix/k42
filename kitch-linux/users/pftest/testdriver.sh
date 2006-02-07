#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: testdriver.sh,v 1.3 2001/03/19 01:25:51 bob Exp $
# ############################################################################

#set -x

case $1 in
    0)
	testbody.sh init &
	wait
	;;
    1)
	testbody.sh &
	wait
	;;
    2)
	testbody.sh &
	testbody.sh &
	wait
	;;
    3)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
    4)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
    5)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
    6)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
    7)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
    8)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
	;;
     9)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    10)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    11)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    12)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    13)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    14)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    15)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
        wait
	;;
    16)
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	testbody.sh &
	wait
        ;;
    *) echo "Option not supported";;
esac
