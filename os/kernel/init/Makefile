# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.55 2005/06/28 19:44:41 rosnbrg Exp $
# ############################################################################



# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch



default:: libobjs

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

CLASSES += AsyncTester
SERVE += ServerState InitServer

LIBKERNSRCS += KernelInit.C AsyncTester.C SysFacts.C idt.C memoryMapKern.C \
	kinit.C initKernelMappings.C KernelExit.C $(SERVE:%=X%.C) \
	$(SERVE:%=%.C)


LIBCSRCS +=

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
