#!/bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kore.sh,v 1.6 2005/08/20 22:51:47 bseshas Exp $
# ############################################################################

export TCLLIBPATH=~bala/tclreadline/lib
export KORE_LIBDIR=~bala/k42/kitchsrc/tools/misc/kore/lib
export KERNEL_IMAGE=~bala/k42/powerpc/fullDeb/os/boot_image.dbg
export MACHINE=kpem

tclsh8.4 $KORE_LIBDIR/tcl/kore.tcl
