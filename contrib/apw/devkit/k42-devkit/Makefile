# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.4 2005/08/04 15:41:38 apw Exp $
# ############################################################################

VERSION=0.5.4

all: k42-devkit_${VERSION}-1_i386.deb

k42-devkit_${VERSION}-1_i386.deb: k42-devkit_${VERSION}.orig.tar.gz
	tar xzf k42-devkit_${VERSION}.orig.tar.gz
	cp -r debian k42-devkit-${VERSION}
	rm -rf k42-devkit-${VERSION}/debian/CVS
	cd k42-devkit-${VERSION} && dpkg-buildpackage -us -uc -rfakeroot

k42-devkit_${VERSION}.orig.tar.gz: README k42-devkit.sh
	mkdir -p k42-devkit-${VERSION}
	cp $^ k42-devkit-${VERSION}
	tar czf k42-devkit_${VERSION}.orig.tar.gz k42-devkit-${VERSION}

clean:
	rm -rf k42-devkit-${VERSION} *.deb *.diff.gz *.dsc* *.changes

publish: all
	scp *.deb buildwookie.austin.ibm.com:public_html/pub/k42/k42-devkit
