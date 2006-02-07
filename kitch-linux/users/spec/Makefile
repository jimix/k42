# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.39 2005/07/17 21:18:54 butrico Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR


SPEC_HOME=$(MKKITCHROOT)/home/spec

output_TARGETS= generic
output_INSTALL= $(output_TARGETS:%=$(SPEC_HOME)/output/%)

tool_TARGETS=	driver	\
		driver.exec

tool_INSTALL= $(tool_TARGETS:%=$(SPEC_HOME)/tools/%) $(SPEC_HOME)/tools/ps

bin_TARGETS=	copy.clone	\
		time		\
		cleanstderr	\
		locale		\
		driver		\
		driver.exec	\
		runSDET		\
		runSDET.Img.3	\
		run		\
		ps		\
		df		\
		mksdet.k42	\
	        run_sdet.k42	\
	        runSDET.Img.4	\
		SDET.filelist.Img.4.cpio \
		k42.filelist.Img.4.cpio

bin_INSTALL= $(bin_TARGETS:%=$(SPEC_HOME)/bin/%)

scripts_TARGETS=

scripts_INSTALL= $(scripts_TARGETS:%=$(SPEC_HOME)/scripts/%)

homescripts_TARGETS=run.sdet
homescripts_INSTALL=$(homescripts_TARGETS:%=$(SPEC_HOME)/%)
home_TARGETS=	M.k42 M.common
home_INSTALL= $(home_TARGETS:%=$(SPEC_HOME)/%)

dot_TARGETS=	profile
dot_INSTALL= $(dot_TARGETS:%=$(SPEC_HOME)/.%)

driver_OBJS=	main.o		\
		get_cmnd.o	\
		bldfile.o	\
		startup.o	\
		str_lookup.o	\
		strsave.o	\
		strtemp.o

driver.exec_OBJS= drvr.exec.setuid.o

tool_OBJS= $(driver_OBJS) $(driver.exec_OBJS) drvr.exec.o
install_OBJS=$(tool_OBJS:%=$(SPEC_HOME)/tools/%)

PKGSRCS	= $(SPEC_PKG)/tools
SUPOBJS =  $($@_OBJS:%=$(PKGSRCS)/%)


ifneq ($(wildcard $(SPEC_PKG)),)
TARGETS= $(tool_TARGETS)
dbg_TARGETS= $(TARGETS:%=%.dbg)
install_TARGETS=	$(tool_INSTALL) $(bin_INSTALL) $(scripts_INSTALL) \
			$(home_INSTALL) $(dot_INSTALL) $(output_INSTALL) \
			$(homescripts_INSTALL)

else

TARGETS=
dbg_TARGETS=
install_TARGETS=
endif

LDLIBS += $($@_LDLIBS)

FIND_CMD=$(FIND) . ! -name \*~ -type f

# target to copy over the benchmark, 1 is a know file
$(SPEC_HOME)/1:
	@$(ECHO) "Copying benchmark to kitchroot."
	cd $(SPEC_PKG) && $(FIND_CMD) -print | $(CPIO) -pdum $(@D)

$(tool_TARGETS):
	$(LINK.c) $(SUPOBJS) $(LDLIBS) -o $@
	$(STRIP.dbg)

# It only matters that these files exist and are older than the excutables
$(install_OBJS): $(SPEC_HOME)/1
	$(TOUCH) $@
#	$(INSTALL_DSK)

$(output_INSTALL): $(SPEC_HOME)/output/%: % $(install_OBJS)
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(tool_INSTALL): $(SPEC_HOME)/tools/%: % $(install_OBJS)
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(bin_INSTALL): $(SPEC_HOME)/bin/%: % $(install_OBJS)
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(scripts_INSTALL): $(SPEC_HOME)/scripts/%: % $(install_OBJS)
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(home_INSTALL): $(SPEC_HOME)/%: $(SRC_DIR_PATH)/% $(install_OBJS)
	$(INSTALL) --mode 0644 $< $@
#	$(INSTALL_DSK)

$(homescripts_INSTALL): $(SPEC_HOME)/%: $(SRC_DIR_PATH)/% $(install_OBJS)
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(dot_INSTALL): $(SPEC_HOME)/.%: $(SRC_DIR_PATH)/% $(install_OBJS)
	$(INSTALL) --mode 0644 $< $@
#	$(INSTALL_DSK)


clobber:
	-$(RM) -r $(SPEC_HOME)/*

clean::
	-$(RM) $(dbg_TARGETS)

install_targets:: $(install_TARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
