# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.32 2005/02/16 00:06:06 mergen Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL= 1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
COMMINCS = sim_bugs.H bugs.H mem_debug.H use_expedient.H optimizations.H \
	   template_bugs.H linux_support.H \
           experimental.H debug_soft_intrs.H \
	   paging.H traceCompile.H pgflt_stats.H lock_options.H

ARCHINCS =

install_includes::
	$(INSTALL_INC) $(INCDIR)/defines/            ./        $(COMMINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

LIBCSRCS +=
LIBKSRCS +=
CLASSES  +=
SRCS     +=
TARGETS  +=

# for kernel modules under the os/kernel tree
LIBKERNSRCS +=

# for server modules under the os/server tree
SERVERS     +=


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
