# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
# All Rights Reserved
#
#  $Id: Makefile,v 1.15 2005/06/28 19:44:17 rosnbrg Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

TESTDIR=pthread
SCRIPTS=

PROGRAMS=	ex1	\
		ex2	\
		ex3	\
		ex4	\
		ex5	\
		ex6	\
		test1   \
		test2	\
		syntheticLock \
		timeSlice

PROGRAMS_32 = $(PROGRAMS:%=%_32)

ifdef IN_OBJ_DIR

USRTARGETS=$(PROGRAMS) $(PROGRAMS_32)

# SRC_DIR_PATH is only defined in the object tree
include $(SRC_DIR_PATH)/../Makefile.tests


$(PROGRAMS): %: %.c
	$(TARGET_CC) -MD $(OPT_FLAGS) $< -lpthread -o $@
	$(STRIP.dbg)

$(PROGRAMS_32): %_32: %.c
	$(TARGET32_CC) -MD $(OPT_FLAGS) $< -lpthread -o $@
	$(STRIP32.dbg)

endif
