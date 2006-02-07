# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.1 2004/02/11 23:03:54 lbsoares Exp $
# ############################################################################

#including appropriate Makefile

ifdef MKSRCTOP
# this means we're building KFS for K42
include $(MKSRCTOP)/os/servers/kfs-unstable/Makefile.k42
else
# building KFS for Linux
include Makefile.linux
endif