#!/bin/sh
#
# Make sure we are run by bash
#
if [ -z "$interp" ]; then
    #
    # Need to make sure we are running bash.
    # We also want to time the script.
    #
    interp=yes
    export interp
    exec sh -c "bash $0 $*"
fi
# ############################################################################
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kfsimage.sh,v 1.3 2004/03/07 00:43:19 lbsoares Exp $
# ############################################################################
#
# This script creates a kfs disk image for a given file hierarchy (directory)
# Usage:
#       kfsimage srcdir diskname
# where:
#       srcdir specfies the file hierarchy to be used to create the KFS image
#       diskname specifies the file name for the image
#

srcdir=$1
diskname=$2

usage() {
    echo "Usage: kfsimage srcdir diskname"
    echo "   srcdir: source directory for the KFS image"
    echo "   diskname: file name for the KFS image"
    exit
}

if [ -z $srcdir ]; then
    usage;
fi

if [ ! -d $srcdir ]; then
    echo "Source directory $srcdir not valid"
    usage;
fi

if [ -z $diskname ]; then
    echo "diskname needs to be specified"
    usage;
fi

MKFS=`type -t mkfs.kfs`
if [ -z $MKFS ]; then
    echo "You need to have mkfs.kfs in your path. The program mkfs.kfs"
    echo "resides in the usual place for K42 tools."
    exit
fi

FSCP=`type -t fscp`
if [ -z $MKFS ]; then
    echo "You need to have the tool fscp in your path. The program fscp"
    echo "resides in the usual place for K42 tools."
    exit
fi

# Creating/formating the disk
echo "Creating disk " $DISK
mkfs.kfs -d $diskname || exit
echo "Disk " $diskname "created"

#copy files
for file in `find $srcdir -type f -o -type l`; do
    dest=${file##$srcdir}
    fscp $diskname $file kfs:$dest  || exit
done

echo "Creation of KFS image (file $diskname) has finished."




