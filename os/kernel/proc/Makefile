# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.65 2004/07/11 21:54:24 andrewb Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

# for installing include files
SYSINCS = ProcessServer.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/sys/            ./        $(SYSINCS)

ifdef IN_OBJ_DIR
#############################################################################
#  rules to be made in object directory
#############################################################################

SERVE += ProcessSetKern ProcessServer


LIBKERNSRCS += ProcessReplicated.C ProcessShared.C \
	Process.C kernRunProcess.C ProgExec.C ProcessDefaultKern.C \
	ProcessVPList.C ProcessVPListArch.C \
	ProcessSetKern.C ProcessServer.C SysCallInitKern.C \
	$(SERVE:%=X%.C)


#############################################################################
#  end of object directory rules
#############################################################################
endif
