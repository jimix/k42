#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: setup.sh,v 1.9 2004/02/18 18:44:42 butrico Exp $
# ############################################################################

# initializes SCSI and wait until it's done

host=`hostname`

case $host in
    k6 | K6)	
	DEV=/dev/scsi/host0/bus0/target9/lun0/part5
	;;
    k9 | K9)	
	DEV=/dev/scsi/host0/bus0/target9/lun0/part4
	;;
    k10 | K10)
	DEV=/dev/scsi/host0/bus0/target8/lun0/part4	
	;;
    *)
#	tar -xvzf /dev/scsi/host0/bus0/target9/lun0/part5
	;;
esac

echo "Z" > /ksys/console
while [ ! -e $DEV ] ;do sleep 1; done


if [ -e /ram/var/tpcw/item.frm ]; then 
exit;
fi

cd /ram
tar -xvzf $DEV

echo "end tar"







