#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: unionfs.sh,v 1.1 2003/09/17 21:25:18 dilma Exp $
# ############################################################################
#

cd /ram; echo "file1" > file1
cd /knfs
rm -rf tmpunion
mkdir tmpunion; cd tmpunion
echo "file2" > file2
mkdir dir; mkdir dir2
/knfs/kbin/unionfsServer /ram /knfs /union
while [ ! -d /union ] ; do true; done

cd /union
# test finding things on primary file system /ram
cat file1
# test finding things on secondary file system /ram
cat tmpunion/file2
# test creating file
echo "fram" > fram
cat fram
# test file creation requiring subdirectories
echo "fknfs" > tmpunion/dir/fknfs

echo "Testing file creation with directory not existing on sec FS (should fail)"
echo "ffail" > tmpunion/fail/ffail

# test mkdir
mkdir foodir

# test mkdir requiring subdirectories
mkdir tmpunion/dir2/foodir

echo "Testing dir creation with subpath not existing on sec FS (should fail)"
echo "fdir" > tmpunion/fdir/fdir

find /ram



