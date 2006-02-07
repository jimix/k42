#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: vhype_install.sh,v 1.4 2004/12/30 23:54:55 mostrows Exp $
# ############################################################################

FILE=$1
VICTIM=$2
USERNAME=`whoami`

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: vhype_install <file1>,<file2>,... <victim>";
    exit 1;
fi

if [ -z "$HW_IMGLOC" ] ;then
    HW_IMGLOC=`kvictim $VICTIM HW_IMGLOC |cut -f2`
fi

while [ -n "$FILE" ] ; do
    f=`echo -n $FILE|cut -f1 -d,`
    if echo $FILE | grep -q -e ',' ; then
	FILE=`echo -n $FILE | cut -f2- -d,`
    else
	FILE=
    fi
    cp $f $HW_IMGLOC/$USERNAME || (echo "Failed to copy $f" ; exit 1);
done

if [ -e "$HW_IMGLOC/$USERNAME/grub.conf" ] ; then
    pushd $HW_IMGLOC/$VICTIM;
    ln -sf ../$USERNAME/grub.conf grub.conf
    popd
fi

exit 0;
