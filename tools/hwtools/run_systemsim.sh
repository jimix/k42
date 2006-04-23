#!/usr/bin/env bash 
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

source ${0%/*}/kconf_lib
set -e

VICTIM=$1
if [ -n "$VICTIM" ]; then
    HW_VICTIM=$VICTIM
fi

export MAMBO_BOOT_FILE=$HW_BOOT_FILE
FILE=$MAMBO_BOOT_FILE

kconf_flatten_export $HW_VICTIM


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
    if [ $OFFSET -ne 0  ] ; then
	dd if=/dev/zero of=$FILE bs=1 seek=$OFFSET conv=notrunc count=$SIZE
	echo -n $HW_CMDLINE | dd of=$FILE bs=1 seek=$OFFSET conv=notrunc
    fi
fi


if [ -z "${MAMBO_TCL_INIT}" -o ! -f "${MAMBO_TCL_INIT}" ] ; then
    export MAMBO_TCL_INIT=`dirname $0`/../lib/mambo${MAMBO_TCL_STAMP}.tcl
    if [ ! -f "${MAMBO_TCL_INIT}" ] ; then
	export MAMBO_TCL_INIT=`dirname $0`/../../lib/mambo${MAMBO_TCL_STAMP}.tcl
    fi
fi



: ${MAMBO_DEBUG_PORT:=1234}

: ${MAMBO_SIMULATOR_PORT:=$[$TW_BASE_PORT-2]}
: ${MAMBO_MEM:=128}
: ${MAMBO_GARB_FNAME:=/dev/zero}
: ${MAMBO_TYPE:=gpul}

: ${MAMBO_ZVAL_START:="---- start ztrace ----"}
: ${MAMBO_ZVAL_STOP:="---- stop ztrace ----"}
: ${MAMBO_ZVAL_FILE:=$PWD/zval.out}


if [ "$MAMBO_DIR" -a -d $MAMBO_DIR ]; then
    export PATH=$MAMBO_DIR/bin:$MAMBO_DIR/bin/emitter:$PATH;
fi


if [ ! "$MAMBO_DIR" ]; then
    if ! which systemsim-${MAMBO_TYPE} >/dev/null 2>/dev/null ; then 
	echo "Can't determine systemsim-${TYPE} location, not in PATH";
	exit 1;
    fi
    MAMBO_EXE=`which systemsim-${MAMBO_TYPE}`
    MAMBO_DIR=${MAMBO_EXE%/bin/systemsim-${MAMBO_TYPE}}
fi

if [ ! "$MAMBO_DIR" ]; then
    if ! which systemsim-${MAMBO_TYPE} >/dev/null 2>/dev/null ; then 
	echo "Can't determine systemsim-${TYPE} location, not in PATH";
	exit 1;
    fi
    MAMBO_EXE=`which systemsim-${MAMBO_TYPE}`
    MAMBO_DIR=${MAMBO_EXE%/bin/systemsim-${MAMBO_TYPE}}
fi

: ${MAMBO_ROM_FILE:=${MAMBO_DIR}/run/${MAMBO_TYPE}/linux/rom.bin}

export MAMBO_TCL_INIT MAMBO_DEBUG_PORT MAMBO_SIMULATOR_PORT 
export MAMBO_MEM MAMBO_GARB_FNAME MAMBO_ROM_FILE MAMBO_BOOT_FILE
export MAMBO_TYPE
export MAMBO_ZVAL_START MAMBO_ZVAL_STOP MAMBO_ZVAL_FILE

if [ -n "$MAMBO_RLWRAP" ]; then
    RLWRAP=`which rlwrap`
    if [ -n "$RLWRAP" ]; then
	RLWRAP="$RLWRAP -r";
    fi
fi

if [ $HW_VERBOSE -ge 1 ] ; then
    echo "(MAMBO_DIR=${MAMBO_DIR})"
fi

exec $RLWRAP systemsim-${MAMBO_TYPE} $MAMBO_EXTRA_OPTS -f $MAMBO_TCL_INIT
