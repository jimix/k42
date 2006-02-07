#! /bin/sh
#
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

# setup Should get these from args
arch=$1 # powerpc
k42=$2  #/u/jimix/work/k42
opt=$3  # fullDeb
glibc=/u/kitchawa/k42-packages/powerpc/glibc/usr-patch7

# Don't touch these

k42libs=${k42}/install/lib/${arch}/${opt}
spec=${k42}/install/tools/AIX/${arch}/${arch}.spec.in
gccincs=${k42}/install/include
gccincs_arch=${k42}/install/gcc-include/arch/${arch}

sed -e "s%@glibc@%$glibc%g; s%@k42libs@%${k42libs}%g; s%@gccincs@%${gccincs}%g; s%@gccincs_arch@%${gccincs_arch}%g;" ${spec}
