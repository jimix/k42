#!/bin/bash
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
VICTIM=$1
CHANNEL=$2
RAWCON=$3

source ${0%/*}/kconf_lib
set -e

ME=$(whoami)

if test "$RAWCON" = "-r"; then
    RAWCON=""
else
    RAWCON="-noraw"
fi


KSERIAL=`kconf_get $VICTIM kserial`
PORT=`kconf_get $VICTIM TW_BASE_PORT`
PORT=$[ $PORT + $CHANNEL]
while true; do 
    if ! hwconsole -s -m $VICTIM |grep $ME ; then
	exit 0;
    fi
    console $RAWCON $KSERIAL:$PORT;
done

    