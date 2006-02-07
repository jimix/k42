#!/bin/bash
#############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: conwrap.sh,v 1.3 2005/08/03 12:05:20 mostrows Exp $
# ############################################################################
#
VICTIM=$1
if [ -n "$VICTIM" ]; then
    HW_VICTIM=$VICTIM
fi

host=${HW_VICTIM}_kserial
port=${HW_VICTIM}_TW_BASE_PORT

exec console -noraw ${!host}:${!port}
