#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kconf_lib.sh,v 1.1 2006/01/18 22:01:34 mostrows Exp $
# ############################################################################

KEYS=()


function kconf_get() {
    set +x
    KEY=$1
    FIELD=$2
    name=${KEY}_${FIELD};

    if [ $HW_VERBOSE -ge 4 ] ; then
	set -x;
    fi

    if [ -z "${!name}" -a "${#KEYS[@]}" -eq "0" ]; then
	eval `kconf -s ${KEY} |\
	    sed  -e "s/^\\([^ ]*\\) \\(.*\\)$/KEYS[\\\${#KEYS[@]}]=\\1; export ${KEY}_\\1=\'\\2\';/g"`
    fi

    if [ $HW_VERBOSE -ge 3 ] ; then
	set -x;
    fi

    if [ -z "${!name}" ]; then
	echo "Can't find $name" >/dev/tty;
	echo -n "#"
    else
	echo -n ${!name}
    fi
}


function kconf_flatten_export() {
    set +x;
    old=$IFS
    IFS=
    base=$1

    if [ $HW_VERBOSE -ge 4 ] ; then
	set -x
    fi

    if [ "${#KEYS[@]}" -eq "0" ]; then
	eval `kconf -s ${base} |\
	    sed -e "s/^\\([^ ]*\\) \\(.*\\)$/KEYS[\\\${#KEYS[@]}]=\\1; export ${base}_\\1=\'\\2\';/g"`
    fi
    for i in ${KEYS[@]} ; do
	name=$base
	name=${name}_$i;
	eval `echo : \\${${i}=\'${!name}\'} \; export ${i}`
    done;
    IFS=$old

    if [ $HW_VERBOSE -ge 3 ] ; then
	set -x;
    fi
}


: ${HW_VERBOSE:=0}
if [ $HW_VERBOSE -ge 3 ] ; then
    set -x;
fi


