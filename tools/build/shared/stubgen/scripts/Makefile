# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.15 2001/06/21 21:53:21 peterson Exp $
# ############################################################################

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

AWK_SCRIPTS = fixupname.awk genprog.awk genhdr.awk genmangle.awk \
	gentmp.awk xcoff_type.awk

$(MKTOOLBIN)/stubgen:  $(MKTOOLBIN)
	$(INSTALL) --mode 0775 -d $@



ifdef IN_OBJ_DIR

# ############################################################################
#
# Makefile rules for making shell scripts from the source shell scripts
# SH_SCRIPTS is the name (foo) of the desired script.
# We take the foo.sh source file, and transform it into foo in the
# object tree.  Then we install the object foo into the toolbin.
#
#  This one is different because it goes in a subdirectory of toolbin,
#  and not into the toolbin itself.

# run the script.sh thru the sed file to instantiate a working script
stubgen: stubgen.sh $(MKTOOLBIN)/script.sed
	$(SED) -f $(MKTOOLBIN)/script.sed < $< > $@

# install the working script into the tool bin
$(MKTOOLBIN)/stubgen/stubgen: stubgen $(MKTOOLBIN)/script.sed
	$(INSTALL) --mode 0555  $< $(MKTOOLBIN)/stubgen

endif


install_includes:: $(MKTOOLBIN)/stubgen $(MKTOOLBIN)/stubgen/stubgen
	$(INSTALL) --mode 0555 $(AWK_SCRIPTS) $(MKTOOLBIN)/stubgen


clean::
	-$(RM) *~

install_includes::
