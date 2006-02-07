#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: startall.sh,v 1.5 2003/08/05 05:17:56 kdiaa Exp $
# ############################################################################


/usr/libexec/mysqld --datadir=/ram/var \
    --basedir=/usr \
    --max_connections=1000 \
    --skip-grant \
    --skip-locking \
    --pid-file=/ram/var/pid.file \
    --log=/ram/var/mysql.log  \
    >>/ram/var/mysqld.err 2>&1 </dev/null &
