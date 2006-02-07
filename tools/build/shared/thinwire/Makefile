# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.33 2005/03/05 02:36:52 mostrows Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

install_includes::
	$(INSTALL) --mode 0444 thinwire-src/simip.h $(MKKITCHTOP)/include/sys

ifdef IN_OBJ_DIR
# FIXME: define a makefile macro that traslates to/from .exe suffix
ifneq ($(findstring CYGWIN,$(PLATFORM_OS)),)
CONSOLE = console.exe
THINWIRE = thinwire.exe
THINWIRE2 = thinwire2.exe
THINWIRE2 = thinwire3.exe
TTYWIRE = ttywire.exe
SIMIP = simip.exe
else # is not windows
CONSOLE = console
THINWIRE = thinwire
THINWIRE2 = thinwire2
THINWIRE3 = thinwire3
TTYWIRE = ttywire
SIMIP = simip
endif # is windows

TARGETS += $(CONSOLE) $(THINWIRE) $(SIMIP) \
	$(THINWIRE2) $(TTYWIRE) $(THINWIRE3)

$(CONSOLE): thinwire-src/console.c
	$(HOST_CC) -Wall -O -g $(HOST_CCFLAGS) $< -o $@

$(THINWIRE): thinwire-src/thinwire.c
	$(HOST_CC) -Wall -O -g $(HOST_CCFLAGS) $< -o $@

$(THINWIRE3): thinwire-src/thinwire3.c
	$(HOST_CC) -Wall -O -g $(HOST_CCFLAGS) $< -o $@

$(TTYWIRE): thinwire-src/ttywire.c
	$(HOST_CC) -Wall -O -g $(HOST_CCFLAGS) $< -o $@

$(THINWIRE2): $(THINWIRE)
	cp $< $@

$(SIMIP): thinwire-src/simip.c thinwire-src/simip.h
	$(HOST_CC) -Wall -O -g $(HOST_CCFLAGS)  $< -o $@

# FIXME: should use INSTALL_INC
install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
# clean obj directory so alternat aix/linux builds work
	$(RM) $(TARGETS)

endif # IN_OBJ_DIR
