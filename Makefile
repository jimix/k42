# ############################################################################
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.110 2005/07/08 01:19:52 apw Exp $
# ############################################################################

##################################################################
#
#   Top-level makefile for K42
#   Commands:
#
#	configuration -- tell us what we are configured to do
#	full_snapshot -- complete build
#	fast_snapshot -- build, assuming previous build
#	clean         -- delete (most) build by-products
#
#   Internal commands:
#
#	install_makefiles -- basic makefile required pieces
#	configure_sesame  -- generates Make.config in each directory
#	install_tools     -- build tools (compile and install)
#	install_includes  -- gather include headers into "install"
#	install_genhdrs   -- generate stub headers
#	libobjs           -- build libraries
#	install_libobjs   -- install libraries
#	install_targets   -- install target programs
#
#

##############################################################################
#
# Define parallel build options.
#

# This must be done in Makefile.kitch and Makerules.kitch.
# We try to determine if a parent has been invoked with parallel
# build options.  If so, then we don't pass -j, -l to children --
# MAKE is smart enough to figure out what is going on.

ifneq ($(PAR_OPTIONS),1)
ifndef LOAD_LIMIT
export LOAD_LIMIT=16
export PROCS=4
endif

PAR_OPTIONS:=1
PARMAKE= $(MAKE) $(PARALLEL)
SEQMAKE= $(MAKE) $(SEQUENTIAL)

SEQUENTIAL=-j 1 SEQ=1

ifneq ($(PAR),1)
  PAR=1
  PARALLEL=-j $(PROCS) -l $(LOAD_LIMIT) PAR=1
else
  export PARALLEL=
endif

ifeq ($(SEQ),1)
 export SEQ=1
 export PARALLEL=$(SEQUENTIAL)
 export PAR=0
endif
endif

SUBDIRS = lib os bin usr kitch-linux

##################################################################
#
#   Determine the platform on which we are running
#
ifndef IN_OBJ_DIR
export PLATFORM_OS:=$(shell uname -s)

ifeq ($(PLATFORM_OS),Linux)
  export PLATFORM:=$(PLATFORM_OS)_$(shell uname -m)
else
  export PLATFORM:=$(PLATFORM_OS)
endif

# make IRIX look like IRIX64 platform
ifeq ($(PLATFORM),IRIX)
  export PLATFORM:=IRIX64
endif

HACKPWD=/bin/pwd

##################################################################
#
#   Include basic configuration information
#
export MKANCHOR := $(strip $(shell cd ..; $(HACKPWD)))
-include Make.paths
endif # ! IN_OBJ_DIR

#debug trace for MKANCHOR setting
#foo:=$(shell echo MKANCHOR is $(MKANCHOR) >/dev/tty)
##################################################################
#
#   If we included Make.paths, above, then HAVE_MFINC will be defined.
#   If it is not defined, we didn't have Make.paths, and we need to
#   create it.  Create if from a user-specific file ~/.kitchmfinc
#   if that exists, or the build default version in MakePaths.proto
#
#   Once we have defined Make.paths, we exit the makefile.
#   BUT -- special feature of GNU make is that if we change
#   the makefile (or any of its included makefiles), make
#   will automatically re-do the command, so we will come
#   back in a second time, but this time Make.paths WILL be there
#   and we will do the "else" clause below.
#
ifndef HAVE_MFINC

#  NOTE: if we don't have Make.paths, then we don't have any
#        makefile variables for commands.  Hence, we have to use
#        raw commands: "echo" and not "@$(ECHO)"

Make.paths: MakePaths.proto
	@echo "Make.paths not found - being created"
	@if [ ! -f $(HOME)/.kitchmfinc ]; then \
		cat Make.paths.proto > Make.paths; \
		echo "by copying Make.paths.proto"; \
	else    \
		cat $(HOME)/.kitchmfinc > Make.paths; \
		echo "by copying $(HOME)/.kitchmfinc"; \
	fi

else # HAVE_MFINC

##################################################################
#
#   We have the basic configuration info from Make.paths, so build.
#
#   Two main options:   full_snapshot or fast_snapshot (default).
#                       fast_snapshot is a subset of full_snapshot.
#

TOP_DIR_HACK=1

default:: fast_snapshot

##################################################################
#
#   From configure_sesame
#   provides MKANCHOR (top of build tree), MKKITCHTOP (install root)
#
.PHONY: Make.config
-include Make.config


##################################################################
#
#   In case we need to know what we are doing, an echo of the state
#
.PHONY: configuration
configuration:
	@$(ECHO) 'platform      $$(PLATFORM) = $(PLATFORM)'
	@$(ECHO) 'platform OS   $$(PLATFORM_OS) = $(PLATFORM_OS)'
	@$(ECHO) 'architecture  $$(OBJDIR_ARCH_TYPES) = $(OBJDIR_ARCH_TYPES)'
	@$(ECHO) 'optimization  $$(OBJDIR_OPT_TYPES) = $(OBJDIR_OPT_TYPES)'
	@$(ECHO) 'build anchor  $$(MKANCHOR) = $(MKANCHOR)'



##################################################################
#
#  make directories if needed
#

$(MKTOOLBIN):
	$(INSTALL) -m 0775 -d $@

ifdef IN_OBJ_DIR
# Must bracket this so we make sure that MKKITCHROOT is known
$(MKKITCHROOT):
	$(INSTALL) -m 0775 -d $@

install_kitchroot: 
	$(MAKE) -C os install_images
	$(INSTALL) -d --mode 0755 $(MKKITCHROOT)/etc 
	$(INSTALL) -d --mode 0755 $(MKKITCHROOT)/ketc 
	$(INSTALL) -d --mode 0755 $(MKKITCHROOT)/kbin
	$(INSTALL) -d --mode 0755 $(MKKITCHROOT)/klib
	$(INSTALL) -d --mode 0755 $(MKKITCHROOT)/tests

# Make kitchroot and kitchroot/bin EARLY
else
install_kitchroot:
	@for i in $(OBJDIRS); do					\
		$(MAKE) -C $$i  $@;					\
	done
endif


##################################################################
#
#  definitions and rules for install_makefiles
#
# mega-hack for installing makefiles

.PHONY: install_makefiles
.PHONY: $(MKTOOLBIN)/script.sed
.PHONY: $(MKKITCHTOP)/lib/Make.help

_MAKE_INCS = Makefile.kitch Make.help Makerules.kitch Makefile.tools \
	     Makerules.tools Make.configure

##################################################################
#
#  Create a sed script to use on any shell script files
#
#  When we define a tool which is a shell script, we may want to modify
#  it for this specific build. For example, the shell that we use could
#  be /bin/sh, /bin/bash, or /bin/ksh ...  To allow this level of
#  customization, we use variables of the form @name@ in the shell
#  (which is named "foo.sh") and then pass the script thru a sed script
#  which converts the script and installs it where it is needed.
#  The shell script ($(MKTOOLBIN)/script.sed) consists of a number
#  of substitutions for the build tools defined in Make.paths	
#

ALL_TOOLS :=    MKANCHOR	\
		MKTOOLBIN	\
		SHELL_CMD	\
		INSTALL		\
		HOST_CC		\
		HOST_CXX	\
		TARGET_CC	\
		TARGET_CXX	\
		TARGET_AS	\
		TARGET_LD	\
		TARGET_AR	\
		TARGET_NM	\
		TARGET_STRIP	\
		EXTRACTPROG	\
		LEX		\
		YACC		\
		MAKE		\
		EXIT		\
		ECHO		\
		TEST		\
		TOUCH		\
		TRUE		\
		AWK		\
		CAT		\
		CHMOD		\
		CMP		\
		CP		\
		CPIO		\
		DD		\
		EMACS		\
		ETAGS		\
		FIND		\
		LN		\
		M4		\
		MV		\
		OPTIMIZATION	\
		RM		\
		SED		\
		SORT		\
		JAR		\
		JAVAC		\
		INSTALL_INC	\
		KNEWEST		\
		STUBGEN		\
		MKSERV		\
		LINUXSRCROOT	\
		K42_PACKAGES    \
		K42_PKGROOTPATH	\
		K42_PKGVER	\
		K42_PKGHOST	\
		K42_KFS_DISKPATH \
		K42_SITE	\
		TFFORMAT	\
		TFINSTALL

# for each tool in the above list, create a sed script substitute
# command that will convert @TOOL@ to the current value of $(TOOL)
# The vertical bar is used to delimit each line.  We want each
# sed substitute command to be on a separate line

ALL_TOOLS_SED = $(foreach tool, $(ALL_TOOLS), "s;@$(tool)@;$($(tool));g|")

$(MKTOOLBIN)/script.sed: $(MKTOOLBIN)
	$(ECHO) $(ALL_TOOLS_SED) | tr '|' '\012' | $(SED) "s/^  *//" > sed.tmp
	$(CMP) -s sed.tmp $(MKTOOLBIN)/script.sed \
	    || $(INSTALL) -m 0444 sed.tmp $(MKTOOLBIN)/script.sed
	$(RM) sed.tmp


##################################################################
#
#  These tools should really be installed from the directory they
#  are in, but we think we need them in order to create the system
#  that would be used to install them.  So we install them here,
#  by hand.


$(MKTOOLBIN)/kinstall: tools/build/shared/kinstall.sh $(MKTOOLBIN)/script.sed
	$(SED) -f $(MKTOOLBIN)/script.sed < tools/build/shared/kinstall.sh > $@
	$(CHMOD) 0755 $@

$(MKTOOLBIN)/knewest: tools/build/shared/knewest.sh $(MKTOOLBIN)/script.sed
        # sub-hack - knewest needs to be installed before install_includes run
	$(SED) -f $(MKTOOLBIN)/script.sed < tools/build/shared/knewest.sh > $@
	$(CHMOD) 0755 $@

ifndef K42TOOLS
INSTALL_MAKEFILES_DEP = $(MKTOOLBIN) $(MKTOOLBIN)/kinstall $(MKTOOLBIN)/knewest
endif

install_makefiles: $(INSTALL_MAKEFILES_DEP)
	$(INSTALL_INC) $(MKKITCHTOP)/lib/ lib/ $(_MAKE_INCS)

##################################################################
#
#  definitions and rules for install_tools
#
.PHONY: install_tools
install_tools::
	cd tools && $(MAKE) install_includes && $(MAKE) install_targets

##################################################################
#
#  configure_sesame, install_includes, install_genhdrs, libobjs,
#  install_libobjs, and install_targets  are all defined in
#  lib/Makerules.kitch  which is included later.
#

#
#  This is the major part of the build system.  We need
#  to make multiple passes over the source tree, in order
#  to build the entire system, either full or fast.  The
#  idea is that we first install headers, then libraries,
#  then programs.
#
#  There are two ways to sequence things in make files: dependencies,
#  and recursive invocations of make.  It is preferable to use
#  dependencies.  However, we have a problem with SEQ.  SEQ=1 means no
#  parallel make; SEQ = 4 means we can run multiple makes in parallel.
#  (We actually build on a 4-way MP, so this can really work).
#
#  But if you run SEQ=4 without the complete dependencies, you
#  can end up building things out of order.  So, at the moment,
#  we need to run some parts with SEQ=1 and we do that by a
#  recursive invocation of make.

.PHONY: step1 step2 step3 step4 step4b step5
.PHONY: fast_header full_header
.PHONY: fast_snapshot full_snapshot

step1:
	@$(ECHO) "---------- make install_makefiles"
	$(MAKE) install_makefiles
	@$(ECHO) "---------- make install_makefiles"


step2:
	@$(ECHO) "---------- make configure_sesame"
	$(MAKE) $(SEQUENTIAL) configure_sesame _CWD=$(MKSRCTOP)
ifndef K42TOOLS
	cd tools && $(MAKE) $(SEQUENTIAL) configure_sesame _CWD=$(MKSRCTOP)/tools
endif

step3:
	@$(ECHO) "---------- make install_includes"
	$(MAKE) $(PARALLEL) install_includes

step4:
	@$(ECHO) "---------- make install_tools "
	$(MAKE) $(SEQUENTIAL) install_tools

step4b:
	@$(ECHO) "---------- make install_kitchroot"
	$(MAKE) $(PARALLEL) install_kitchroot

step5:
	@$(ECHO) "---------- make install_genhdrs"
	$(MAKE) $(SEQUENTIAL) install_genhdrs
	@$(ECHO) "---------- make install_libobjs "
	$(MAKE) $(PARALLEL) install_libobjs
	@$(ECHO) "---------- make install_targets"
	$(MAKE) $(PARALLEL) install_targets


fast_header:
	@$(ECHO) "----------  DOING PARTIAL SNAPSHOT:  ------------"
	@$(ECHO) "-- RUN make full_snapshot if you have problems --"

full_header:
	@$(ECHO) "--------- DOING FULL SNAPSHOT FROM TOP ----------"

fast_snapshot: fast_header step1
	$(MAKE) step3    # install_includes
	$(MAKE) step5    # install_genhdrs, libraries, targets


full_snapshot: full_header step1
	$(MAKE) step2        # configure_sesame
	$(MAKE) step3        # install_includes (SEQ=1 may be unnecessary)
ifndef K42TOOLS
	$(MAKE) step4        # install_tools
endif
	$(MAKE) step4b	     # install kitchroot
	$(MAKE) step5        # install_genhdrs, libraries, targets


#
#
# This is "full_snapshot", but stop after headers installed
#
headers: full_header step1
	$(MAKE) step2
	$(MAKE) step3


#rule to build clean version of disk
rebuild_disk:
	rm -rf /k42/okrieg/current/install/powerpc/fullDeb/kitchroot
	rm /k42/okrieg/current/powerpc/fullDeb/os/DISK0.0.0
	$(MAKE) SEQ=1 step4b	     # install kitchroot
	$(MAKE) SEQ=1 step5        # install_genhdrs, libraries, targets	

##################################################################
#
#
kuserinit::
	@ksh tools/build/shared/kuserinit


##################################################################
#
#  clean -- main content is defined in tools/lib/Makerules.kitch below
#
ifndef IN_OBJ_DIR
clean::
	cd tools && $(MAKE) clean
endif

##################################################################
#
#  cscope -- generate cscope files
#
cscope:
	find . -name "*.[chCHIS]" > cscope.files
	cscope -k -b -q

.PHONY: tags
tags: TAGS

TAGS:
	find . -name '*.[CcHh]' -print | etags -


##################################################################
#
#
# hack for top-level; see also makerules: target
# make sure don't include this if I am in the object directory
# since path is wrong there

ifndef IN_OBJ_DIR
include ./lib/Makerules.kitch
endif #IN_OBJ_DIR


endif # HAVE_MFINC
