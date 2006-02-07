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

ME=$(whoami)

if test "$RAWCON" = "-r"; then
    RAWCON=""
else
    RAWCON="-noraw"
fi

VIC=($(kvictim $VICTIM kserial TW_BASE_PORT))
KSERIAL=${VIC[1]}
PORT=${VIC[2]}
PORT=$[ $PORT + $CHANNEL]
while true; do 
    if ! hwconsole -s -m $VICTIM |grep $ME ; then
	exit 0;
    fi
    console $RAWCON $KSERIAL:$PORT;
done

    