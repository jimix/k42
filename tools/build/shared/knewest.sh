#!/bin/sh

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: knewest.sh,v 1.5 2001/04/02 16:49:57 peterson Exp $
# ############################################################################

#make all files date of newest in list
if [ -z "$1" ]
then
    echo make all files time same as newest
    exit 99
fi
newest=$1
for x in $* ;
do
   if [ $x -nt $newest ] 
   then 
          newest=$x
   fi
done
#echo $newest newest
touch -r $newest $*
