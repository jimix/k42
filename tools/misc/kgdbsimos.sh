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
#  $Id: kgdbsimos.sh,v 1.6 2003/01/24 16:36:40 jimix Exp $
# ############################################################################

#
# Initialize K42_SIMOS_DEBUG_PORT if it's not already defined in the
# environment.
#
. kuservalues
let debug_port=$K42_SIMULATOR_PORT+10
: ${K42_SIMOS_DEBUG_PORT:=$debug_port}

#
# Build a temporary .gdbinit file that targets simos on the right port.
#
rm -f .gdbinit_$$
echo "target simos localhost:$K42_SIMOS_DEBUG_PORT" > .gdbinit_$$

#
# Try to locate a gdbsrcdirs file.  If found, source it from the .gdbinit file.
#
srcdirs=$(echo ${PWD#*Deb/} | sed -e 's;[^/][^/]*;..;g')/os/gdbsrcdirs
if [[ -r $srcdirs ]]; then
    echo "source $srcdirs" >> .gdbinit_$$
fi

#
# Run gdb, passing all command line arguments through.
#
case $(uname -s) in
AIX)
    powerpc64-linux-gdb --command .gdbinit_$$ $*
    ;;
IRIX|IRIX64)
    tgdb --command .gdbinit_$$ $*
    ;;
*)
    ;;
esac

rm -f .gdbinit_$$
