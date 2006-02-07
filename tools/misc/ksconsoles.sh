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
#  $Id: ksconsoles.sh,v 1.4 2003/01/24 16:36:41 jimix Exp $
# ############################################################################

. kuservalues

#
# Construct a few default values in local variables.
#
let console_port=$K42_SIMULATOR_PORT+20
simos_dir=/u/kitchawa/local/simos/bin

#
# Assign default values to anything that is not already defined.
#
: ${K42_SIMOS_CPUS:=1}
: ${K42_SIMOS_CONSOLE_PORT:=$console_port}
: ${SIMOS_DIR:=$simos_dir}

#
# Process command line, possibly overriding default or environment values.
#
CMD=$0

verbose=0
if [[ $# -ge 1 && "$1" = "-verbose" ]]; then
    verbose=1
    shift
fi

if [[ $# -ge 2 && "$1" = "-cpus" ]]; then
    K42_SIMOS_CPUS=$2
    shift 2
fi

if [[ $# -gt 0 ]]; then
    echo "
Usage: $CMD [-verbose] [-cpus <number>]

    Environment variables:
	K42_SIMOS_CPUS          - number of cpus
	K42_SIMOS_CONSOLE_PORT  - starting port for auxiliary consoles
	SIMOS_DIR               - directory where simos executable lives

    Default values are provided for any variables not already defined
    in the environment.  Command-line arguments are provided for a few
    of the variables.
    "
    exit -1
fi

if [[ $verbose = 1 ]]; then
    echo "K42_SIMOS_CPUS:           $K42_SIMOS_CPUS"
    echo "K42_SIMOS_CONSOLE_PORT:   $K42_SIMOS_CONSOLE_PORT"
    echo "SIMOS_DIR:                $SIMOS_DIR"
fi

PATH=$PATH:$SIMOS_DIR

let i=1	# skip cpu 0
while (( i < $K42_SIMOS_CPUS )); do
    let port=$K42_SIMOS_CONSOLE_PORT+$i
    xterm -iconic -title "sconsole $i" -e sconsole $(hostname) $port &
    let i=i+1
done
