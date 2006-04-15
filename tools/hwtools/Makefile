# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

ifndef SITE
$(error Must define SITE)
endif

BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib

SCRIPTS=hwconsole ktwd kvictim reboot_kpower run_simip run_mambo \
	run_systemsim \
	vhype_install kernel_install conloop reboot_serial ksitevalues \
	pxe_install kbladed conwrap lpar_prep lpar_run thinwire_lpar \
	thinwire_hw thinwire_blade thinwire_mambo reboot_kpower_ozlabs \
	reboot_k0 shutdown_k0

FILES=victims.conf watson.conf torolab.conf toronto.conf ozlabs.conf \
	lanl.conf bluewookie_austin.conf \
	arl.conf arl_austin.conf generic_hw.conf mambo-120305.tcl \
	mambo.tcl mambo-120604.tcl mambo-05222005.tcl utils.tcl

INSTALL=install

all: install_scripts install_files

install_files: $(LIBDIR) $(FILES:%=$(LIBDIR)/%)


PID:=$(shell echo $$$$)

$(LIBDIR)/victims.conf: victims.conf
	cp victims.conf /tmp/victims.conf.$(PID)
	echo "include $(SITE).conf" >> /tmp/victims.conf.$(PID)
	$(INSTALL) -m 0644 /tmp/victims.conf.$(PID) $@
	rm /tmp/victims.conf.$(PID)

$(LIBDIR)/% : % 
	$(INSTALL) -m 0644 $< $@

install_scripts: $(BINDIR) $(SCRIPTS:%=$(BINDIR)/%)


$(BINDIR)/% : %.pl 
	$(INSTALL) -m 0755 $< $@

$(BINDIR)/% : %.sh
	$(INSTALL) -m 0755 $< $@


$(LIBDIR):
	$(INSTALL) -m 0775 -d $@

$(BINDIR):
	$(INSTALL) -m 0775 -d $@
