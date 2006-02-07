# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.3 2000/05/11 11:29:19 rosnbrg Exp $
# ############################################################################

# dummy to do all makes in parent - used in machine dependent children
.SUFFIXES:
.DEFAULT:
	-(cd ../.. && $(MAKE) $@)

.PHONY: default
default:
	-(cd ../.. && $(MAKE))
