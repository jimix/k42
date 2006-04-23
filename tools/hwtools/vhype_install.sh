#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: vhype_install.sh,v 1.6 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################

FILE=$1
VICTIM=$2
USERNAME=`whoami`

source ${0%/*}/kconf_lib
set -e

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: vhype_install <file1>,<file2>,... <victim>";
    exit 1;
fi

if [ -z "$HW_INSTALL" ] ;then
    HW_INSTALL=`kconf_get $VICTIM HW_INSTALL`
fi

while [ -n "$FILE" ] ; do
    f=`echo -n $FILE|cut -f1 -d,`
    if echo $FILE | grep -q -e ',' ; then
	FILE=`echo -n $FILE | cut -f2- -d,`
    else
	FILE=
    fi
    $HW_INSTALL $VICTIM $f $USERNAME
done

if [ -e "$HW_IMGLOC/$USERNAME/grub.conf" ] ; then
    pushd $HW_IMGLOC/$VICTIM;
    ln -sf ../$USERNAME/grub.conf grub.conf
    popd
fi

exit 0;
