#!/usr/bin/env bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: klogin.sh,v 1.5 2004/09/15 16:46:04 marc Exp $
# ############################################################################

eval $(kuservalues)
let K42_LOGIN_PORT=$USR_BASE_PORT+13
export K42_LOGIN_PORT
exec k42login $*

