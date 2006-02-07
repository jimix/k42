# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.7 2004/01/30 21:58:23 aabauman Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools


ifdef IN_OBJ_DIR

TARGETS += k42login

HOST_CFLAGS += -Dlint

k42login: k42login.c err.h
	$(HOST_CC) $(HOST_CFLAGS) $(CPPFLAGS) $< -o $@

# FIXME: should use INSTALL_INC
install_targets:: $(TARGETS)
	@$(ECHO) "$(TARGETS) must be set execute suid root"
	@$(ECHO) "$(INSTALL) --mode 0444 $(TARGETS) $(MKTOOLBIN)"
	@$(INSTALL) --mode 0444 $(TARGETS) $(MKTOOLBIN) ; \
	if [ $$? -ne 0 ] ; then \
	  @$(ECHO) "Warning:  $(TARGETS) not installed.  Probably in use." ; \
	fi
	$(RM) $(TARGETS)

clean::
	-$(RM) $(OBJS) *~

endif # IN_OBJ_DIR

