# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2001/06/19 17:21:31 peterson Exp $
# ############################################################################

SUBDIRS =
default:: targets

# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += servers.list

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
