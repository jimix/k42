# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2005/05/10 22:18:58 apw Exp $
# ############################################################################

VERSION=0.1.0

all: k42-toolchain_${VERSION}-1_i386.deb

k42-toolchain_${VERSION}-1_i386.deb: k42-toolchain-${VERSION}/debian/control

k42-toolchain-${VERSION}/debian/control: debian/control \
                                         k42-toolchain_${VERSION}.orig.tar.gz
	tar xzf k42-toolchain_${VERSION}.orig.tar.gz
	cp -r debian k42-toolchain-${VERSION}
	cd k42-toolchain-${VERSION} && dpkg-buildpackage -us -uc -rfakeroot

source: k42-toolchain_${VERSION}.orig.tar.gz

k42-toolchain_${VERSION}.orig.tar.gz:
	wget http://buildwookie.austin.ibm.com/~apw/pub/k42/k42-devkit/k42-toolchain_0.1.0.orig.tar.gz

clean:
	rm -rf k42-toolchain-${VERSION} *.gz *.deb *.dsc* *.changes

publish: all
	scp *.deb buildwookie.austin.ibm.com:public_html/pub/k42/k42-devkit
