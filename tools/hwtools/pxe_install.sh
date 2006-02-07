#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: pxe_install.sh,v 1.3 2005/08/09 22:16:25 mostrows Exp $
# ############################################################################

FILE=$1
VICTIM=$2
USERNAME=`whoami`

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: pxe_install <file> <victim>";
    exit 1;
fi

if [ -z "$HW_IMGLOC" ] ;then
    HW_IMGLOC=`kvictim $VICTIM HW_IMGLOC |cut -f2`
fi

cp $FILE $HW_IMGLOC/pxe/images/bzImage.$VICTIM
chmod a+rw $HW_IMGLOC/pxe/images/bzImage.$VICTIM

exit 0;
