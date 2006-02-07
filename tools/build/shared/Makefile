# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.35 2005/06/07 03:46:38 jk Exp $
# ############################################################################

SUBDIRS = stubgen mkserv thinwire relocate k42login fstools genmap

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools


##################################################################
#
#  definitions and rules for installing the three build scripts
#

SH_SCRIPTS:=	kinstall knewest cvsimport

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
