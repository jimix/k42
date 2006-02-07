# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.36 2004/05/07 15:44:34 bob Exp $
# ############################################################################

# SUBDIRS =
# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

COMMINCS = trace.H traceIncs.H traceUnified.H traceBase.H \
	   traceUtils.H traceCtr.H

install_includes:: 
	$(INSTALL_INC) $(INCDIR)/trace/          ./          $(COMMINCS)

# some implicit rules for making stub stuff
# these overwrite some potential rules from the global includes

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

CLASSES +=

LIBKERNSRCS += trace.C tracePrint.C traceUtils.C

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
