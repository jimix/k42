#!/bin/sh
#
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file
# LICENSE.html in the top-level directory for more details.

set -e

ssh -1 -n thinwire@kserial /home/thinwire/bin/panelctl /dev/ttyS0 SAMIPowerOff
