#!/usr/bin/env bash 
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi


export MAMBO_BOOT_FILE=$HW_BOOT_FILE

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

[ -z $MAMBO ] && MAMBO=$(type systemsim-${MAMBO_TYPE} | awk '{print $3}');
[ -z $MAMBO ] && MAMBO=$(type mambo-${MAMBO_TYPE} | awk '{print $3}');

if [ -z $MAMBO ]; then
	echo "run_mambo: FAIL: could not find mambo executable"
	exit 1
fi

if [ ! "$MAMBO_DIR" ]; then
    MAMBO_DIR=$(dirname $(dirname $(type $MAMBO | awk '{print $3}')))
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
    echo "`ls -ld ${MAMBO_DIR}`"
    echo "`ls -l ${MAMBO_DIR}/bin/mambo-${MAMBO_TYPE}`"
    echo "Exec of: $RLWRAP mambo-${MAMBO_TYPE} $MAMBO_EXTRA_OPTS \
	    -f $MAMBO_TCL_INIT"
fi


exec $RLWRAP $MAMBO $MAMBO_EXTRA_OPTS -f $MAMBO_TCL_INIT
