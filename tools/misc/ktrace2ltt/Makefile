# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2004/04/14 20:00:33 aabauman Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

CONV_TARGET = ktrace2ltt
CONV_OBJS = ktrace2ltt.o traceUtils.o traceCommon.o

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += $(CONV_TARGET)
HOST_CXXFLAGS += -MD -I$(MKKITCHTOP)/include -I$(MKSRCTOP)/tools/misc/kmon

$(CONV_TARGET):	$(CONV_OBJS)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

%.o: $(MKSRCTOP)/os/kernel/trace/%.C
	$(HOST_CXX) -c $(HOST_CXXFLAGS) $< -o $@

%.o: $(MKSRCTOP)/tools/misc/kmon/%.C
	$(HOST_CXX) -c $(HOST_CXXFLAGS) $< -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)

clean::
	-$(RM) $(CONV_OBJS) $(CONV_OBJS:%=%.d)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif # IN_OBJ_DIR
