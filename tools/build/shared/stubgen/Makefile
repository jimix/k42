# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.7 2001/06/19 17:22:12 peterson Exp $
# ############################################################################

# set KERNEL=1 if for kernel
KERNEL=1

# test directory is left off since it requires the actuall system be
#  build first
# currently we are only building for x86 where yacc is not required %HF

#SUBDIRS = yacc grammar extract scripts test
SUBDIRS = elf grammar extract scripts #test

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools
