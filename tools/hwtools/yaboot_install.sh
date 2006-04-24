#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: yaboot_install.sh,v 1.1 2006/04/12 01:24:25 butrico Exp $
# ############################################################################


FILE=$1
VICTIM=$2
USERNAME=`whoami`

source ${0%/*}/kconf_lib
set -e

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: yaboot_install <file> <victim>";
    exit 1;
fi

kconf_flatten_export $VICTIM

if [ -z "$HW_INSTALL" ] ;then
    HW_INSTALL=`kconf_get $VICTIM HW_INSTALL`
fi

tmp=$(tempfile)

function cleanup() {
    rm $tmp
}

trap cleanup EXIT

if [ -z "$machine" ] ; then
    machine=$VICTIM;
fi

# generate the yaboot conf file from a template and the command line
cat > $tmp <<EOF
init-message="Enter 'install' to boot the installer, or 'install-safe' if you have problems booting"

device=enet:
timeout=10
default=k42

image=chrpboot.${machine}
        label=k42
        append="$HW_CMDLINE"

EOF


yaboot_config=yaboot.conf.${machine}
$HW_INSTALL ${VICTIM} $tmp ${yaboot_config}
exec kernel_install ${FILE} ${VICTIM}
