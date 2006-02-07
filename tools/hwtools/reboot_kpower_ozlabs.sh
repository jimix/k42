#!/bin/sh

# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

echo rebooting $1
POWER_BAR=`kvictim impago kpower | cut -f2`
echo -e "/boot $1\r\n/x\r\n" | nc $POWER_BAR 23 > /dev/null
