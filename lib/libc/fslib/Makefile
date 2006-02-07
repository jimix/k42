# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: Makefile,v 1.32 2005/04/08 18:21:07 okrieg Exp $
# ############################################################################

default:: libobjs

SUBDIRS = virtfs

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
INCFILES = NameTreeLinuxFS.H NameTreeLinuxFSVirtFile.H DirLinuxFS.H \
	   ServerFile.H MultiLinkManager.H FileSystemGlobal.H FSFile.H \
	   FSStats.H FreeList.H DirLinuxFSVolatile.H FSCreds.H FSNode.H \
	   ServerFileBlock.H NameTreeLinuxFSVirtFile.H \
	   fs_defines.H direntk42.H DentryList.H FileSystemList.H \
	   PagingTransport.H PagingTransportPA.H PagingTransportVA.H

CLASSES = FSNode

ARCHINCS =

install_includes::
	$(INSTALL_INC) $(INCDIR)/fslib/           ./        $(INCFILES)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/..
LIBFSSRCS += NameTreeLinuxFS.C DirLinuxFS.C \
	     ServerFile.C ServerFileSharing.C ServerFileBlock.C \
	     DirLinuxFSVolatile.C FileSystem.C FSNode.C FreeList.C \
	     NameTreeLinuxFSVirtFile.C DentryList.C FSStats.C \
	     PagingTransport.C PagingTransportPA.C PagingTransportVA.C \
	     FSFileOther.C

LIBKERNSRCS += FSNode.C NameTreeLinuxFS.C ServerFile.C ServerFileBlock.C

LIBKSRCS += FSNode.C

# ############################################################################
#  end of object directory rules
# ############################################################################
endif



