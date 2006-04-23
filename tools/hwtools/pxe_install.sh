#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: pxe_install.sh,v 1.5 2006/01/18 22:01:35 mostrows Exp $
# ############################################################################


FILE=$1
VICTIM=$2
USERNAME=`whoami`

source ${0%/*}/kconf_lib
set -e

if [ -z "$FILE" -o -z "$VICTIM" ] ; then
    echo "Usage: pxe_install <file> <victim>";
    exit 1;
fi

if [ -z "$HW_INSTALL" ] ;then
    HW_INSTALL=`kconf_get $VICTIM HW_INSTALL`
fi

tmp=$(tempfile)

function cleanup() {
    rm $tmp
}

trap cleanup EXIT

if [ "$HW_CMDLINE_FILE" -a -r "$HW_CMDLINE_FILE" ] ; then
    cmdline="	append $(cat $HW_CMDLINE_FILE)";
fi
if [ "$HW_CMDLINE" ] ; then
    cmdline="	append $HW_CMDLINE";
fi

#SERIAL 1 ${VICTIM}_serial1_speed 0
cat > $tmp <<EOF
default netboot

label netboot
	kernel images/bzImage.$VICTIM
$cmdline

prompt 1
timeout 1
EOF

echo $HW_IMGLOC
boot_config=${VICTIM}_boot_config
$HW_INSTALL $VICTIM $FILE pxe/images/bzImage.$VICTIM
$HW_INSTALL $VICTIM $tmp ${!boot_config}
exit 0;
