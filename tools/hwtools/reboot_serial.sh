#!/bin/sh
# Copyright (C) 2004,  National ICT Australia (NICTA)
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

# This script reboots a Power4-class machine by sending a magic string
# on the primary serial port which is monitored by the service processor.
# To get this to work you need to configure serial port snooping in
# the service processor menus, select serial port 1, and configure
# reboot_port, reboot_baud and reboot_string in victims.conf.

if [ $# != 1 ]; then
	echo "Usage: $0 victim"
	exit 1
else
	VICTIM=$1
fi

# find out parameters for victim
TMP="$(kvictim $VICTIM kserial reboot_port reboot_baud reboot_string)"
if [ "$TMP" == "" ]; then
	echo "$0: error getting info for $VICTIM from kvictim"
	exit 1
fi

HOST=$(echo "$TMP" | cut -f2)
PORT=$(echo "$TMP" | cut -f3)
BAUD=$(echo "$TMP" | cut -f4)
STRING=$(echo "$TMP" | cut -f5)

if [ "$HOST" != $HOSTNAME -a "$HOST" != $(echo $HOSTNAME | cut -d. -f1) ]; then
	echo "$0: sorry, I don't support rebooting non-local victims yet"
	exit 1
fi

if [ ! -w "$PORT" ]; then
	echo "$0: error: can't write to $PORT"
	exit 1
fi

# set the baud rate, and send the magic string
stty -F $PORT $BAUD && echo -n $STRING > $PORT
