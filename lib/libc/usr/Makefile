# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.70 2005/06/28 19:44:30 rosnbrg Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

LIBTARGETS += ProgExecUsr.o

COMMINCS = ProgExec.H runProcessCommon.H
SYSINCS = GDBIO.H

# for installing machine specific includes
ARCHINCS  = ProgExecArch.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/usr/             ./           $(COMMINCS)
	$(INSTALL_INC) $(INCDIR)/sys/ 		 ./           $(SYSINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/..

COMMONSRCS += gdb-stub.C memoryMap.C runProcessCommon.C ProgExecCommon.C

LIBCSRCS += $(COMMONSRCS) userRunProcess.C \
	    FIXMEx86LibInit.c GDBIO.C ProgExec.C
LIBKSRCS += $(COMMONSRCS) $(LIBNATVSRCS) GDBIOKern.C
CLASSES  +=
SRCS     +=
TARGETS  +=

LDFLAGS = -r

$(LIBDIR):
	$(INSTALL) --mode 0775 -d $@

install_libobjs:: $(LIBDIR)

$(LIBDIR)/%.o: %.o
	$(INSTALL) --mode 0444 $< $@

install_libobjs:: $(LIBTARGETS) $(LIBTARGETS:%=$(LIBDIR)/%)

clean::
	-$(RM) $(LIBTARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
