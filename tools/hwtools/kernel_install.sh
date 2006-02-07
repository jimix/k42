#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kernel_install.sh,v 1.6 2005/08/09 21:58:50 mostrows Exp $
# ############################################################################

# Make sure any error is reflected back to caller.
set -e

FILE=$1
VICTIM=$2

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: kernel_install <file> <victim>";
    exit 1;
fi

if [ -z "$HW_IMGLOC" ] ;then
    HW_IMGLOC=`kvictim $VICTIM HW_IMGLOC |cut -f2`
fi

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
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

if [ "$HW_CMDLINE_FILE" -a -r "$HW_CMDLINE_FILE" ] ; then
    OFFSET=$(hex2dec $(readelf -W -S $FILE | \
	    grep __builtin_cmdline |get_field 5 ))
    if [ $OFFSET -ne 0  ] ; then
	CLEANUP=`tempfile`
	cp $FILE $CLEANUP
	dd if=/dev/zero of=$CLEANUP bs=1 seek=$OFFSET conv=notrunc count=512
	dd if=$HW_CMDLINE_FILE of=$CLEANUP bs=1 seek=$OFFSET conv=notrunc
	FILE=$CLEANUP
    fi
fi

SITE=`kvictim site name |cut -f2`
case $SITE in
    *torolab*)
	cp $FILE /tftpboot/$VICTIM;
	;;

    *watson*)
	USERNAME=`whoami`
	cp $FILE $HW_IMGLOC/chrpboot.$USERNAME.$VICTIM ;
	chmod o+r $HW_IMGLOC/chrpboot.$USERNAME.$VICTIM || {
	    echo "WARNING: chmod failed, continuing boot anyway ...";
	}
	ln -sf chrpboot.$USERNAME.$VICTIM $HW_IMGLOC/chrpboot.$VICTIM;
	;;

    *toronto*)
	ln -sf $FILE /guest/kitchawa/tftpboot/chrpboot.$VICTIM ;
	;;
    *ozlabs*)
    	scp $FILE $HW_IMGLOC/zImage.$VICTIM
	;;

    *)
	cp $FILE $HW_IMGLOC/chrpboot.$VICTIM;
        ;;
esac

if [ "$CLEANUP" ] ; then
    rm $CLEANUP;
fi

exit 0;
