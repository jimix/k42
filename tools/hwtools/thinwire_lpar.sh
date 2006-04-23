#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: thinwire_lpar.sh,v 1.3 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################

LPAR=$1

source ${0%/*}/kconf_lib
set -e

if [ -z "$LPAR" ] ; then
    echo "Usage: thinwire_lpar <lpar_name> <ignored....>";
    exit 1;
fi

base=$[$TW_BASE_PORT-1]

exec thinwire3 localhost:$base stdin `seq $[$base+2] $[$base+5]`  

