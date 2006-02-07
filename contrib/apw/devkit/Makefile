# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.3 2005/02/20 04:20:31 apw Exp $
# ############################################################################

all:
	make -C k42-devkit
	make -C k42-toolchain
	make -C k42-packages

clean:
	make -C k42-devkit clean
	make -C k42-toolchain clean
	make -C k42-packages clean

publish:
	make -C k42-devkit publish
	make -C k42-toolchain publish
	make -C k42-packages publish
