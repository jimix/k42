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
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kmaketags.sh,v 1.33 2003/01/24 16:36:40 jimix Exp $
# ############################################################################

# kmaketags [architecture]
#
# makes a tags file in kernel and each server under servers
# which links to tags files in lib and include
# works when called from ksh under aix or from bash under windows.
# Some C-shell user should figure out how to test for C-shell without breaking 
# bash/ksh and put in code to call bash to run this.


PLATFORM=$(uname -s)

# make IRIX look like IRIX64 platform
if [ $PLATFORM = "IRIX" ]; then
    PLATFORM=IRIX64
fi                                                                                       

if [ -n "$1" ]
then
    arch=$1
elif [ "$PLATFORM" = AIX ]
then
    arch=powerpc
else
    echo "architecture is undefined"
    exit 98
fi

case $arch in
  powerpc)
	     filter=generic64\\/\|amd64\\/;;
  amd64)
	     filter=generic64\\/\|powerpc\\/;;
  *) echo "kmaketags [architecture]"
     echo "  architectures are powerpc, mips64, amd64"
     exit 99;;
esac

#set MKANCHOR
. kanchor

if [ -z $MKANCHOR ]
then
    echo Run in a directory containing Make.config or
    echo set environment MKANCHOR to the build anchor
    exit 99
fi
   
KITCHSRC=${MKANCHOR}/kitchsrc/
KITCHTOP=${MKANCHOR}/install/

includes=''
for x in ${KITCHSRC}lib ${KITCHSRC}kitch-linux ${KITCHSRC}os/servers ${KITCHTOP}include 
do
cd $x
find . -type f -print | egrep '\.[cChHsS]$|\.el$' | egrep -v $filter | \
	etags --regex '/_C_LABEL(.*)/' -
includes="$includes --include $x"
done

#do kernel
cd ${KITCHSRC}os/kernel
find . -type f -print | egrep '\.[cChHsS]$|\.el$' | egrep -v $filter | \
        egrep -v genConstDefs.C | \
	etags $includes \
	--regex '/_C_LABEL(.*)/' -
includes="$includes --include ${KITCHSRC}os/kernel"

cd ${KITCHSRC}os/boot
find . -type f -print | egrep '\.[cChHsS]$|\.el$' | egrep -v $filter | \
	etags --regex '/_C_LABEL(.*)/' -

