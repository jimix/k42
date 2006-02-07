#!/usr/bin/env bash
#
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file
# LICENSE.html in the top-level directory for more details.

set -e

ssh -1 -n thinwire@kserial /home/thinwire/bin/panelctl /dev/ttyS0 SAMIPowerOff
while true; do
    echo "waiting 1 minute for poweroff to take effect ..."
    sleep 60
    ssh -1 -n thinwire@kserial \
        /home/thinwire/bin/panelctl /dev/ttyS0 StatusRequest | 
            egrep 'f2020901|f2020909' && break;
done
ssh -1 -n thinwire@kserial /home/thinwire/bin/panelctl /dev/ttyS0 SAMIPowerOn

for min in 10 9 8 7 6 5 4 3 2 1; do
  echo "expecting to get a console on k0 in $min minutes ...";
  sleep 60;
done

echo "you should have a console on k0 within 1 minute";
