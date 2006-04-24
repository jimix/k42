#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: hw_install.sh,v 1.4 2006/01/18 22:01:34 mostrows Exp $
# ############################################################################

VICTIM=$1
SRCFILE=$2
DESTFILE=$3

source ${0%/*}/kconf_lib
set -e

if [ -z "$HW_IMGLOC" ] ;then
    HW_IMGLOC=`kconf_get $VICTIM HW_IMGLOC`
fi

case ${HW_IMGLOC} in
    ssh://*)
	tmp=${HW_IMGLOC#*://}
	host=${tmp%%/*}
	dest=${tmp#*/}
	chmod a+r $SRCFILE || true
	scp -p $SRCFILE $host:$dest/$DESTFILE
	;;
    *)
	rm -f $HW_IMGLOC/$DESTFILE
	cp $SRCFILE $HW_IMGLOC/$DESTFILE
	chmod a+r $HW_IMGLOC/$DESTFILE || /bin/true
;;
esac
