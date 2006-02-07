# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.

time=$1
profile_mask=0x31808 
( echo $profile_mask ) > /ksys/traceMask
sleep $time
echo time $time
(echo 0x0) > /ksys/traceMask
exit
