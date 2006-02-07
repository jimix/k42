#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# ############################################################################
#
# Simple tests for virtfs (framework for procfs-like file systems). It relies
# on /usr/testVirFile to produce the "server" for a file.
#

if [ -e /virtfs/tmp ]; then
    echo -e "Cleaning up /virtfs/tmp"
    rm -rf /virtfs/tmp
fi

echo -e "Creating directory /virtfs/tmp"
mkdir /virtfs/tmp


echo -e "server for /virtfs/tmp/v1 will be created, sleep to wait for it"
/testVirtFile /virtfs/tmp/v1 100 &

sleep 5;

echo -e "file /virtfs/tmp/v1 created"

echo -e "contents of /virtfs/tmp/v1 being cat to /tmp/v1.out"
cat /virtfs/tmp/v1 > /tmp/v1.out
echo -e "content of /tmp/v1.out is:"
cat /tmp/v1.out
echo -e "\n"

echo -e "Open truncate the virtfile (and writing '?' as its data)\n"
echo "?" > /virtfs/tmp/v1
cat /virtfs/tmp/v1

echo -e "writing '*' as data to file (with open O_APPEND)\n"
echo "*" >> /virtfs/tmp/v1
cat /virtfs/tmp/v1


echo -e "Trying to remove directory that does not exist (should fail)\n"
rmdir /virtfs/tmp/foo

echo -e "Trying to remove file with rmdir (should fail)\n"
rmdir /virtfs/tmp/v1

echo -e "Tring to remove directory with simple unlink (should fail)\n"
rm /virtfs/tmp

echo -e "Creating directories (/virtfs/tmp/foo and /virtfs/tmp/subfoo) and "
echo -e "removing them\n"
mkdir /virtfs/tmp/foo
rmdir /virtfs/tmp/foo
mkdir /virtfs/tmp/foo
mkdir /virtfs/tmp/foo/subfoo
ls -l /virtfs/tmp/
rm -rf /virtfs/tmp/foo
ls -l /virtfs/tmp

echo -e "Removing file /virtfs/tmp/v1\n"
rm /virtfs/tmp/v1
ls -l /virtfs/tmp

