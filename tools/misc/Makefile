# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.81 2005/04/27 18:45:11 apw Exp $
# ############################################################################

SUBDIRS = kmon ktrace2ltt

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools


# ############################################################################

SH_SCRIPTS:=	kmaketags	\
		makepatch	\
		mkspec		\
		kanchor		\
		knfsroot	\
		kuservalues	\
		kgdbsimos	\
		ksconsoles	\
		klogin		\
		knightly	\
		k42-2-linux	\
		sdetPrep	\
		dezig		\
		traddr          \
                kfsimage



PL_SCRIPTS:=	jiggy		\
		k42console	

SUPPORT_FILES=	k42.tcl		\
		k42_env_desc.txt

SH_SCRIPTS+=	daylog		\
		send-cvs-logs	



# install the shell scripts
TOOL_SCRIPTS:=$(addprefix $(MKTOOLBIN)/, $(SH_SCRIPTS) $(PL_SCRIPTS))
install_targets:: $(TOOL_SCRIPTS)

ifdef IN_OBJ_DIR


# ############################################################################
#
# Makefile rules for making shell scripts from the source shell scripts
# SH_SCRIPTS is the name (foo) of the desired script.
# We take the foo.sh source file, and transform it into foo in the
# object tree.  Then we install the object foo into the toolbin.
#

# run the script.sh thru the sed file to instantiate a working script
$(SH_SCRIPTS): %: %.sh $(MKTOOLBIN)/script.sed
	$(SED) -f $(MKTOOLBIN)/script.sed < $< > $@

$(PL_SCRIPTS): %: %.pl $(MKTOOLBIN)/script.sed
	$(SED) -f $(MKTOOLBIN)/script.sed < $< > $@


# install the working script into the tool bin
$(TOOL_SCRIPTS): $(MKTOOLBIN)/%: %
	$(INSTALL) --mode 0555  $< $(MKTOOLBIN)


# install the support files
$(MKTOOLLIB):
	$(INSTALL) -m 0775 -d $@


install_targets:: $(MKTOOLLIB)

install_targets:: $(SUPPORT_FILES) | $(MKTOOLLIB)
	$(INSTALL) --mode 0444  $? $(MKTOOLLIB)


endif





CVSDIFFS = cvsdiffs
NEN = nextErrNumber
TP = traceParse
SP = serrorParse
PERFMON = perfMon
GDB2TRC = gdb2trc
ADDNOTE = addnote
BUILDKERNPARMS = build_kern_parms

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += $(CVSDIFFS) $(NEN) $(SP) $(PERFMON) $(GDB2TRC) $(ADDNOTE) $(BUILDKERNPARMS)

HOST_CXXFLAGS += -MD -I $(MKKITCHTOP)/include

$(CVSDIFFS): cvsdiffs.C
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

$(NEN): nextErrNumber.C
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

$(TP): 	traceParse.C
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

$(SP): 	serrorParse.C
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

$(PERFMON): perfMon.C
	$(HOST_CXX) -g -Wall $(HOST_CXXFLAGS) $< -o $@

$(GDB2TRC): gdb2trc.c
	$(HOST_CC) -Wall $(HOST_CFLAGS) $< -o $@

$(ADDNOTE): 	addnote.c
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

$(BUILDKERNPARMS): build_kern_parms.c
	$(HOST_CXX) -Wall $(HOST_CXXFLAGS) $< -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
# clean obj directory after install to make aix/linux work
	$(RM) $(TARGETS)

clean::
	-$(RM) $(TARGETS:%=%.o) $(TARGETS:%=%.d)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
