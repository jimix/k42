#!/bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: k42-2-linux.sh,v 1.2 2003/05/09 16:33:54 mostrows Exp $
# ############################################################################

#
# Usage:  k42-2-linux  <path-to-k42-fs>  <new dir name>
#
# Copies specified K42 root FS image to "new dir name", and 
# adjusts the resulting image to allow it to be used as a
# "chroot" target on ppc64-linux
#


SRC=$1
DEST=$2

cp -a $SRC $DEST

cd $DEST/lib64
cp /lib64/libc.so.6 .
cp /lib64/ld64.so.1 .
cp /lib64/ld64.so.1 ld-2.2.5.so
cp /lib64/libpthread.so.0 .
cp /lib64/libpthread.so.0 libpthread.so
cd ../usr
ln -sf lib lib64

mkdir $DEST/tmp
chmod 0777 $DEST/tmp
chmod a+t $DEST/tmp

