#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: lpar_prep.sh,v 1.3 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################

FILE=$1
LPAR=$2

source ${0%/*}/kconf_lib
set -e

if [ -z "$FILE" -o -z "$LPAR" ] ; then
    echo "Usage: lpar_prep <file> <lpar_name>";
    exit 1;
fi

exec hype_prep_lpar -C -n $LPAR -i $FILE 
