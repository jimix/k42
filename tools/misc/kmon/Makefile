# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.26 2005/01/27 20:48:14 bob Exp $
# ############################################################################

default:: targets

JVSRC = C.java CDAColorWindow.java CDAInfo.java Global.java M.java \
	MsgDialog.java PIDColorWindow.java PIDInfo.java PrintfFormat.java \
	CommIDColorWindow.java CommIDInfo.java \
	TraceFilter.java ZoomWindow.java \
	ShowEvent.java ShowEventCount.java ShowEventType.java ShowString.java \
	TraceEvent.java g.java getEventType.java traceDraw.java

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

CXXTARGETS = traceTool traceProfile traceParse
COMMON_OBJS = traceUtils.o traceCommon.o

## only try to build kmon if we have a java compiler
#ifeq ($(notdir $(shell which $(JAVAC))),$(notdir $(JAVAC)))

# only build on AIX for now, which has a known working javac
ifeq ($(PLATFORM_OS),AIX)
KM = kmon.jar
else
KM =
endif


##################################################################
#
#  definitions and rules for installing the runkmon shell script
#

SH_SCRIPTS := runkmon tracePostProc runtraceProfile

# install the shell scripts
TOOL_SCRIPTS:=$(addprefix $(MKTOOLBIN)/, $(SH_SCRIPTS))
install_includes:: $(TOOL_SCRIPTS)

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

# install the working script into the tool bin
$(TOOL_SCRIPTS): $(MKTOOLBIN)/%: %
	$(INSTALL) --mode 0555  $< $(MKTOOLBIN)

endif


CLEANFILES+=*.class $(TTOBJS) $(TPOBJS) *.d *.java *.jar kmonMainClass *.o dumpTraceStrings
CLEANDIRS=kmonPkg

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += $(CXXTARGETS) $(KM) traceNetClient

HOST_CXXFLAGS += -MD -I $(MKKITCHTOP)/include -Wall -g -O0

traceUtils.o: $(MKSRCTOP)/os/kernel/trace/traceUtils.C
	$(HOST_CXX) -c $(HOST_CXXFLAGS) $(CPPFLAGS) $< -o $@

%.o: %.C
	$(HOST_CXX) -c $(HOST_CXXFLAGS) $(CPPFLAGS) $< -o $@

$(CXXTARGETS) dumpTraceStrings: %: %.o $(COMMON_OBJS)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

traceNetClient: traceNetClient.o
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

%.class: 	$(SRC_DIR_PATH)/%.java
#		CLASSPATH=$(<D):. $(JAVAC) -deprecation -d . $<

traceStrings.java:	dumpTraceStrings
			./dumpTraceStrings
			mkdir -p kmonPkg
			mv traceStrings.java kmonPkg

%.java:		$(SRC_DIR_PATH)/kmonPkg/%.java
		mkdir -p kmonPkg
		cp $(SRC_DIR_PATH)/kmonPkg/$@ ./kmonPkg

kmon.class:	traceStrings.java $(JVSRC)
		cp $(SRC_DIR_PATH)/kmon.java .
		cp $(SRC_DIR_PATH)/kmonMainClass .
		$(JAVAC) -deprecation -d . kmon.java

$(KM): 	kmon.class
	$(JAR) cmf kmonMainClass $@ *.class kmonPkg/*.class

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
	$(RM) $(TARGETS)
	$(RM) *.o *.d

-include dumpTraceStrings.d

clean::
	-$(RM) $(CLEANFILES)
	-$(RM) -r $(CLEANDIRS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
