# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.25 2004/08/20 17:30:45 mostrows Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL= 1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
COMMINCS = syscalls.H thinwire.H entryPoints.H Dispatcher.H VolatileState.H \
	   memoryMap.H SoftIntr.H KernelInfo.H KernelScheduler.H
ARCHINCS = syscalls.H VolatileState.H memoryMap.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/sys/            ./        $(COMMINCS)

ifdef IN_OBJ_DIR
#############################################################################
#  rules to be made in object directory
#############################################################################

LIBCSRCS += symdefs.raw
LIBKSRCS += symdefsKern.raw
CLASSES  +=
SRCS     +=
TARGETS  +=

# for kernel modules under the os/kernel tree
LIBKERNSRCS += hcall.S

# for server modules under the os/server tree
SERVERS     +=

-include genSymdefs.d

genSymdefs.o: genSymdefs.C 
	$(HOST_CXX) -MD -DTARGET_$(TARGET_MACHINE) \
		-DTARGET_MACHINE=$(TARGET_MACHINE) -I. -c $< -o $@
# kludge to make alternate builds on aix an linux work
# MAA Jan 18, 1004
	grep -v '/aix/\|/usr/include' genSymdefs.d > genSymdefs.nd
	mv genSymdefs.nd genSymdefs.d


genSymdefs: genSymdefs.o
	$(HOST_CXX) -I. $< -o $@

symdefs.raw: genSymdefs
	./genSymdefs -user > $@

symdefsKern.raw: genSymdefs
	./genSymdefs -kernel > $@

clean::
	-$(RM) genSymdefs genSymdefs.d symdefs.raw symdefsKern.raw

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
