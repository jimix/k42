#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: testbody.sh,v 1.3 2003/05/01 15:05:45 mostrows Exp $
# ############################################################################

one_test() {
    #/bin/sh -c 'echo $$.'
    grep thisisnotinfile grepdata
}

test_chunk() {
    for j in 1 2 3 4; do one_test; done;
}

if [ $# = 1 ]; then
    one_test
    one_test
else
    for j in `seq $1` ; do one_test ; done;
fi
