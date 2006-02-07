# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.11 2001/06/19 17:21:53 peterson Exp $
# ############################################################################

SUBDIRS = fix-symtab instrument-asm

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

SCRIPTS= rmcr remcr

install_includes::
	$(INSTALL) --mode 0555 $(SCRIPTS) $(MKTOOLBIN)
