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

if [ "$MAMBO_DIR" -a -d $MAMBO_DIR ]; then
    export PATH=$(cd $MAMBO_DIR && pwd -P)/bin:$PATH;
    export PATH=$(cd $MAMBO_DIR && pwd -P)/bin/emitter:$PATH;
fi

: ${MAMBO_DEBUG_PORT:=1234}
: ${MAMBO_SIMULATOR_PORT:=$[$TW_BASE_PORT-2]}
: ${MAMBO_MEM:=256}
: ${MAMBO_GARB_FNAME:=/dev/zero}
: ${MAMBO_TYPE:=gpul}

: ${MAMBO_ZVAL_START:="---- start ztrace ----"}
: ${MAMBO_ZVAL_STOP:="---- stop ztrace ----"}
: ${MAMBO_ZVAL_FILE:=$PWD/zval.out}


if [ -z "$MAMBO" ] ; then
    if type systemsim-${MAMBO_TYPE}; then
	tmp=($(type systemsim-${MAMBO_TYPE}));
	MAMBO=${tmp[2]}
    elif type mambo-${MAMBO_TYPE}; then
	tmp=($(type mambo-${MAMBO_TYPE}));
	MAMBO=${tmp[2]}
    fi
fi

if [ -z $MAMBO ]; then
	echo "run_mambo: FAIL: could not find mambo executable"
	exit 1
fi

if [ ! "$MAMBO_DIR" ]; then
    MAMBO_DIR=$(dirname $(dirname $(type -P $MAMBO)))
fi

: ${MAMBO_ROM_FILE:=${MAMBO_DIR}/run/${MAMBO_TYPE}/linux/rom.bin}

export MAMBO_TCL_INIT MAMBO_DEBUG_PORT MAMBO_SIMULATOR_PORT 
export MAMBO_MEM MAMBO_GARB_FNAME MAMBO_ROM_FILE MAMBO_BOOT_FILE
export MAMBO_TYPE MAMBO_DIR
export MAMBO_ZVAL_START MAMBO_ZVAL_STOP MAMBO_ZVAL_FILE

if [ -n "$MAMBO_RLWRAP" ]; then
    RLWRAP=`which rlwrap`
    if [ -n "$RLWRAP" ]; then
	RLWRAP="$RLWRAP -r";
    fi
fi

if [ $HW_VERBOSE -ge 1 ] ; then
    echo "(MAMBO_DIR=${MAMBO_DIR})"
    echo "`ls -ld ${MAMBO_DIR}`"
    echo "`ls -l ${MAMBO_DIR}/bin/mambo-${MAMBO_TYPE}`"
    echo "Exec of: $RLWRAP mambo-${MAMBO_TYPE} $MAMBO_EXTRA_OPTS \
	    -f $MAMBO_TCL_INIT"
fi

exec $RLWRAP $MAMBO $MAMBO_EXTRA_OPTS -f $MAMBO_TCL_INIT
