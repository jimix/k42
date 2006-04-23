#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: lpar_run.sh,v 1.4 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################

LPAR=$1

source ${0%/*}/kconf_lib
set -e

if [ -z "$LPAR" ] ; then
    echo "Usage: lpar_run <lpar_name>";
    exit 1;
fi

: ${LPAR_MEM:=256MB:256MB}

base=$[$TW_BASE_PORT-1]
hype_term  -n $LPAR -p $base  & 
sleep 1;

exec hype_run_lpar -n ${LPAR} -m ${LPAR_MEM}
