#!/bin/bash
#############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: conwrap.sh,v 1.4 2006/01/18 22:01:34 mostrows Exp $
# ############################################################################
#
source ${0%/*}/kconf_lib
set -e

VICTIM=$1
if [ -n "$VICTIM" ]; then
    HW_VICTIM=$VICTIM
fi

host=`kconf_get ${HW_VICTIM} kserial`
port=`kconf_get ${HW_VICTIM} TW_BASE_PORT`

exec console -noraw ${host}:${port}
