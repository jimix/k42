#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: pmu_run.sh,v 1.4 2005/08/22 21:49:01 bob Exp $
# ############################################################################
#

#./pmu.sh config  -e 2 -R 1000000  -G 31,16 -G 50,16,100
./pmu.sh config  -e 2 -R 1000000  -L 10000000 -G 32,16 -G 31,16 
./pmu.sh start -m sampling  
$1 $2 $3 $4 $5 $6
./pmu.sh stop sampling
/kbin/tracedServer --dump

