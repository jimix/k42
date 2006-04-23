#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kernel_install.sh,v 1.9 2006/03/10 01:17:13 mostrows Exp $
# ############################################################################


FILE=$1
VICTIM=$2

source ${0%/*}/kconf_lib

# Make sure any error is reflected back to caller.
set -e

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: kernel_install <file> <victim>";
    exit 1;
fi


if [ -z "$HW_INSTALL" ] ;then
    HW_INSTALL=`kconf_get $VICTIM HW_INSTALL`
fi

# This code tries to insert HW_CMDLINE_FILE into the boot image, if
# the image has a section called "__builtin_cmdline".  We assume the
# thing is 512 bytes in size
function hex2dec () {
    echo $1 | perl -ne 'print hex($_) . "\n";'
}

function get_field () {
    ( read -a X ; echo ${X[$1]};)
}


kconf_flatten_export $VICTIM

if [ -z "$HW_CMDLINE" ] ; then
    if [ "$HW_CMDLINE_FILE" -a -r "$HW_CMDLINE_FILE" ] ; then
	HW_CMDLINE=`cat $HW_CMDLINE_FILE`
    fi
fi

if [ "$HW_CMDLINE" ] ; then
    OFFSET=$(objdump -h  $HW_BOOT_FILE | \
	    gawk -- '{if($2=="__builtin_cmdline") {print strtonum("0x" $6);}}')
    SIZE=$(objdump -h  $HW_BOOT_FILE | \
	    gawk -- '{if($2=="__builtin_cmdline") {print strtonum("0x" $3);}}')
    if [ "x$OFFSET" != "x" ] ; then 
	if [ "$OFFSET" -ne 0  ] ; then
	    dd if=/dev/zero of=$FILE bs=1 seek=$OFFSET conv=notrunc count=$SIZE
	    echo -n "$HW_CMDLINE" | dd of=$FILE bs=1 seek=$OFFSET conv=notrunc
	fi
    fi
fi

if [ -z "$machine" ] ; then 
    machine=$VICTIM;
fi

SITE=`kconf_get $VICTIM site`
case $SITE in
    *torolab*)
	cp $FILE /tftpboot/$machine;
	;;

    *watson*)
	USERNAME=`whoami`
	$HW_INSTALL $VICTIM $FILE chrpboot.$machine ;
	;;

    *toronto*)
	ln -sf $FILE /guest/kitchawa/tftpboot/chrpboot.$machine ;
	;;
    *ozlabs*)
    	scp $FILE $HW_IMGLOC/zImage.$machine
	;;

    *)
	cp $FILE $HW_IMGLOC/chrpboot.$machine;
        ;;
esac

if [ "$CLEANUP" ] ; then
    rm $CLEANUP;
fi

exit 0;
