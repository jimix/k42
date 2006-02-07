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
#  $Id: knfsroot.sh,v 1.11 2003/01/24 16:36:40 jimix Exp $
# ############################################################################

# determine the root that k42 should be mounting for the file system
# normal use is: K42_NFS_ROOT=$(knfsroot -echo) (or your shell's equivalent)
#            or: . knfsroot                  (from bash or ksh)

#acquire ARCH
case $(uname -s) in
    AIX|Linux)         ARCH=powerpc;;
    IRIX|IRIX64) ARCH=mips64;;
    *)           echo "Undefined ARCHitecture"; exit ;;
esac

#acquire MKANCHOR
MKANCHOR=$(kanchor -echo)

# when undefined, defaults to localhost
: ${K42_NFS_HOST:=} 

# Assumes you are running in a direcetory that conatians the debug level string
DBG_LVL=$(echo $(pwd -P) | sed -ne 's,.*/\([^/]*Deb\).*,\1,p')

K42_NFS_ROOT=
if [[ -n "$MKANCHOR" && -n "$ARCH" ]]; then
    K42_NFS_ROOT=$K42_NFS_HOST:${MKANCHOR}/install/$ARCH/$DBG_LVL/kitchroot
fi

if [ "$1" = -echo ]; then
    echo $K42_NFS_ROOT
fi
