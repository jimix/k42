# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.9 2005/07/22 23:28:56 apw Exp $
# ############################################################################

VERSION=0.5.4
ORIG=k42-packages-${VERSION}
DPKG=k42-packages_${VERSION}
PREFIX=/u/kitchawa/k42-packages
LNXVER=029
IMGVER=4
LEGAL="/* Not cleared for public.  */"

all: $(DPKG)-1_i386.deb

$(DPKG)-1_i386.deb: $(ORIG)/debian/control

$(ORIG)/debian/control: debian/control $(DPKG).orig.tar.gz
	rsync -rl --delete debian/ $(ORIG)/debian/
	cd $(ORIG) && dpkg-buildpackage -b -us -uc -rfakeroot

orig: $(DPKG).orig.tar.gz
$(DPKG).orig.tar.gz: $(ORIG)/.legal
	tar czf $@ $(ORIG)

legal: $(ORIG)/.legal
$(ORIG)/.legal: $(ORIG)/.rsync
	chmod -R u+w $(ORIG)
	cp mambo_bd.c   \
             $(ORIG)/$(PREFIX)/src-cvs/linux-$(LNXVER)/drivers/block/mambo_bd.c
	echo $(LEGAL) > \
             $(ORIG)/$(PREFIX)/src-cvs/linux-$(LNXVER)/drivers/net/mambonet.c
	echo $(LEGAL) > \
             $(ORIG)/$(PREFIX)/src-cvs/linux-$(LNXVER)/include/config/mambo.h
	echo $(LEGAL) > \
             $(ORIG)/$(PREFIX)/src-cvs/linux-$(LNXVER)/include/linux/mambobd.h
	rm -rf $(ORIG)/$(PREFIX)/partDeb.Img.$(IMGVER)/opt/ibmcmp
	touch $(ORIG)/.legal

tree: $(ORIG)/.rsync
$(ORIG)/.rsync:
	mkdir -p $(ORIG)/$(PREFIX)/src-cvs
	mkdir -p $(ORIG)/$(dir $(PREFIX))/k42/src
	rm -f $(ORIG)/$(PREFIX)/fullDeb.Img.$(IMGVER)
	rm -f $(ORIG)/$(PREFIX)/noDeb.Img.$(IMGVER)
	ln -s partDeb.Img.$(IMGVER) $(ORIG)/$(PREFIX)/fullDeb.Img.$(IMGVER)
	ln -s partDeb.Img.$(IMGVER) $(ORIG)/$(PREFIX)/noDeb.Img.$(IMGVER)
	rsync -a --delete $(PREFIX)/partDeb.Img.$(IMGVER)/ \
              $(ORIG)/$(PREFIX)/partDeb.Img.$(IMGVER)/
	rsync -a --delete $(dir $(PREFIX))/k42/src/linux-2.4.21-ppc32/ \
              $(ORIG)/$(dir $(PREFIX))/k42/src/linux-2.4.21-ppc32/
	rsync -a --delete \
              --exclude BitKeeper --exclude SCCS --exclude Documentation \
              $(PREFIX)/src-cvs/linux-$(LNXVER)/ \
              $(ORIG)/$(PREFIX)/src-cvs/linux-$(LNXVER)/
	touch $(ORIG)/.rsync

clean:
	rm -rf *.deb *.dsc* *.changes *.orig.tar.gz \
               $(ORIG)/.rsync $(ORIG)/.legal

mrproper: clean
	rm -rf $(ORIG)

publish: all
	scp *.deb apw@kvasir.watson.ibm.com:public_html/pub/k42-devkit
