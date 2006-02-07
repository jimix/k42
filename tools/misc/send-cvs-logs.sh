#!/bin/sh

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: send-cvs-logs.sh,v 1.18 2003/03/07 10:29:32 jimix Exp $
# ############################################################################

#****************DO NOT CHANGE THIS IN /u/kitchawa/bin***********************
#****************UPDATE THE COPY IN tools/misc in the tree*******************
#****************AND COPY TO /u/kitchawa/bin*********************************

KITCHAWAN=/u/kitchawa

export CVSROOT

CVSROOT=$KITCHAWAN/cvsroot

cd $CVSROOT/CVSROOT

TMPFILE=/tmp/send-cvs.$$;

DAYLOG=$KITCHAWAN/bin/daylog;

find=/usr/gnu/bin/find
find_args="-type d -maxdepth 1"

except=" ! (
  -name CVSROOT -o
  -name kitch-core -o
  -name kitch-linux -o
  -name hype -o -name hypesrc -o
  -name vhype-src -o -name vhype-stidc
)"


if [ -s commitlog.today ]; then
    # kitchsrc gets sent separtely
    $DAYLOG kitch-linux kitch-core >$TMPFILE && \
    /usr/bin/mail -s '[CVS-kitchsrc] Daily Kitchawan Commit Logs' \
	kitchawan_regress <$TMPFILE 

    # hype* is no longer used so just send them to jimix
    $DAYLOG hype* >$TMPFILE && \
    /usr/bin/mail -s '[CVS-vhype-stidc] Daily Hypervisor Commit Logs' \
	'jimix' <$TMPFILE 
    # hype-stidc gets sent separately
    $DAYLOG vhype-stidc >$TMPFILE && \
    /usr/bin/mail -s '[CVS-vhype-stidc] Daily Hypervisor Commit Logs' \
	'stidc' <$TMPFILE 
    # hype-src gets sent separately
    $DAYLOG vhype-src >$TMPFILE && \
    /usr/bin/mail -s '[CVS-vhype-src] Daily Hypervisor Commit Logs' \
	'vhype' <$TMPFILE 

    for i in `${find} ${CVSROOT} ${find_args} ${except}  -printf "%f\n"` ; do
	$DAYLOG $i > $TMPFILE.$i && \
	/usr/bin/mail -s "[CVS-$i] Daily Kitchawan Commit Logs" \
	    kitchawan_regress < $TMPFILE.$i 
    done;
    
    cp /dev/null commitlog.today
fi
rm -rf $TMPFILE*;
