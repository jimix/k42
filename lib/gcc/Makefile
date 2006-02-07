# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.11 2005/06/16 19:42:12 apw Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

GCC_HDR_DIR := $(shell $(TARGET_CC) $(TARGET_CFLAGS) --print-file-name=include)

#
# Get a list of all the files in the standard GCC header directory
# Do we really want all of them?
#
generic64_HDRS_0 := $(notdir $(wildcard $(GCC_HDR_DIR)/*.h))
amd64_HDRS_0 = $(generic64_HDRS_0)
mips64_HDRS_0 = $(generic64_HDRS_0)
powerpc_HDRS_0 = $(generic64_HDRS_0)
#
# Phony
#
generic64_HDRS_1 = stdarg.h stddef.h limits.h syslimits.h
amd64_HDRS_1 = $(generic64_HDRS_1)
mips64_HDRS_1 = $(generic64_HDRS_1) va-mips.h
powerpc_HDRS_1 = $(generic64_HDRS_1) va-ppc.h

ifdef GCC3
powerpc_HDRS_1=	iso646.h	\
		limits.h	\
		stdarg.h	\
		stdbool.h	\
		stddef.h	\
		stdlib.h	\
		syslimits.h	\
		varargs.h
endif

HDRS=$($(TARGET_MACHINE)_HDRS_$(TARGET_IS_PHONY))

install_HDRS=$(HDRS:%=$(INC_GCC)/%)

$(INC_GCC):
	$(INSTALL) --mode 0775 -d $@

$(HDRS:%=$(GCC_HDR_DIR)/%): $(INC_GCC)

ifeq ($(GCC34),1)
$(INC_GCC)/stddef.h: $(GCC_HDR_DIR)/stddef.h
	$(INSTALL) --mode 0644 $< $@
	sed \
 's,#ifdef _STDDEF_H,#if defined(_STDDEF_H) \&\& !defined(offsetof),' < $< > $@
endif

ifeq ($(PLATFORM_OS),AIX)
# This is so ugly it is shameful

# How gcc handles stdargs depends on whether it is being compatible
# with AIX or not.  Asserting AIX makes the packages barf so we need
# to make sure that it is only applicable to this file.

$(INC_GCC)/stdarg.h: $(GCC_HDR_DIR)/stdarg.h
	@ echo XXX Doing starg.h/AIX Hack XXX
	@(echo ; \
	 echo "#ifndef _AIX /* _K42_Generated */" ; \
	 echo "#define _K42_Generated" ; \
	 echo "#define _AIX" ; \
	 echo "#endif /* _K42_Generated */"  ; \
	 echo ; cat $< ; echo ; \
	 echo "#ifdef _K42_Generated /*_K42_Generated */" ; \
	 echo "#undef _K42_Generated" ; \
	 echo "#undef _AIX" ; \
	 echo "#endif /* _K42_Generated */") > $@

AIX_hack_HDRS=$(filter-out $(INC_GCC)/stdarg.h, $(install_HDRS))
$(AIX_hack_HDRS): $(INC_GCC)/%: $(GCC_HDR_DIR)/%
	$(INSTALL) --mode 0644 $< $@
else 
ifeq ($(GCC34),1)
$(filter-out $(INC_GCC)/stddef.h, $(install_HDRS)): \
    $(INC_GCC)/%: $(GCC_HDR_DIR)/%
	$(INSTALL) --mode 0644 $< $@
else
$(install_HDRS): $(INC_GCC)/%: $(GCC_HDR_DIR)/%
	$(INSTALL) --mode 0644 $< $@
endif
endif

install_genhdrs:: $(install_HDRS) $(INC_GCC)

#
# Replace these objects in our copy of libgcc.a
#

mips64_SRCS = throw.c

REPLACE_1 = _eprintf.C _eh.C
SRCS = _bb.C $(REPLACE_$(TARGET_IS_PHONY)) $($(TARGET_MACHINE)_SRCS)
REPLACE_OBJS= $(addsuffix .o, $(basename $(SRCS)))
LIBTARGETS += $(REPLACE_OBJS)

orig_LIB_GCC := $(shell $(TARGET_CC) $(TARGET_CFLAGS) -print-libgcc-file-name)

$(LIB_GCC): $(orig_LIB_GCC) $(REPLACE_OBJS)
	$(INSTALL) --mode 0644 $< $@
	$(TARGET_AR) ruv $@ $(REPLACE_OBJS)

libobjs:: $(REPLACE_OBJS)

install_libobjs:: $(LIB_GCC)

endif
