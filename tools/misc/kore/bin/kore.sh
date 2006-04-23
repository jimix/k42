#!/bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kore.sh,v 1.7 2005/10/19 04:12:44 cyeoh Exp $
# ############################################################################

TCLLIBPATH=${K42_TCLLIBPATH:-~bala/tclreadline/lib}
KORE_LIBDIR=${K42_KORE_LIBDIR:-~bala/k42/kitchsrc/tools/misc/kore/lib}
KERNEL_IMAGE=${K42_KERNEL_IMAGE:-~bala/k42/powerpc/fullDeb/os/boot_image.dbg}
MACHINE=${K42_CORE_MACHINE:-kpem}
GDB=${K42_GDB:-gdb}

export TCLLIBPATH
export KORE_LIBDIR
export KERNEL_IMAGE
export MACHINE
export GDB

tclsh8.4 $KORE_LIBDIR/tcl/kore.tcl
