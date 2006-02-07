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
#  $Id: kanchor.sh,v 1.6 2003/01/24 16:36:40 jimix Exp $
# ############################################################################

# determine the anchor of the build that the current directory
# is part of and echo that path
# normal use is: MKANCHOR=$(kanchor -echo) (or your shells equivlant)
#            or: . kanchor                 (from bash or ksh)
#
# returns null if can't find anchor
MKANCHOR=
if [ -r Make.config ]
then
    MKANCHOR=$(grep MKANCHOR Make.config)
    MKANCHOR=${MKANCHOR#*=}
fi
test "$1" = "-echo"  && echo $MKANCHOR

