#!/bin/ksh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: runkmon.sh,v 1.5 2004/01/18 00:53:38 bob Exp $
# ############################################################################

#FIXME there must be a better way of getting all the arguments
#jre -ms16000000 -mx512000000 -cp `which kmon.jar` kmon $1 $2 $3 $4 $5 $6 $7 $8 $9
java -ms16000000 -mx512000000 -jar `which kmon.jar`
