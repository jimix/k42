# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.14 2005/02/10 12:59:26 mostrows Exp $
# ############################################################################

SUBDIRS = etc build misc 


install_includes::
	$(MAKE) -C hwtools LIBDIR=$(MKTOOLLIB) BINDIR=$(MKTOOLBIN) \
			   INSTALL=$(INSTALL) SITE=$(K42_SITE)


-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools
