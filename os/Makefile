# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.51 2004/09/29 21:36:38 dilma Exp $
# ############################################################################

SUBDIRS += kernel boot servers

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default::
ifndef SEQ
	# avoid this extra pass if running sequentially, otherwise
	# do this part in parallel
	$(MAKE) $(PARALLEL) libobjs
endif
	$(MAKE) $(PARALLEL) install_libobjs
	$(MAKE) $(PARALLEL) install_targets

tags:
	@$(ECHO) "making TAGS"
	$(ETAGS)  */*.[cChH] */*/*.[cChH] */*/*/*.[cChH] $(MKKITCHTOP)/obj/*.[cChH] $(MKKITCHTOP)/xobj/*.[cChH] $(MKKITCHTOP)/include/*.[cChH] $(MKKITCHTOP)/include/*/*.[cChH] $(MKKITCHTOP)/include/*/*/*.[chCH]

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += gdbsrcdirs


###############################################################################
#
# File System Configuration
#

KFS_MKFSDISK:
	$(MKFS) -d $(MKFSDISK)
	rm -f $(MKFSPKGDISK)
	ln -s $(K42_KFS_PKGDISK) $(MKFSPKGDISK)


clean_disk:
	rm -f $(MKFSDISK)
	rm -f $(MKFSPKGDISK)
	rm -rf $(MKKITCHROOT)
#
# File System Configuration
#
###############################################################################

ifdef FILESYS
ifeq (KFS,$(FILESYS))
ifdef FILESYS_DISK_IMAGE
FSMKFSDISK=$(FILESYS)_MKFSDISK
else
FSMKFSDISK=
endif
endif
else
FSMKFSDISK=
endif

.PHONY: mkfs.kfs
mkfs.kfs: $(FSMKFSDISK)
install_images: mkfs.kfs

gdbsrcdirs:
	$(FIND) $(MKANCHOR)/kitchsrc -type d -print \
	    | $(SED) \
		-e 's/^/dir /' \
		-e 's;/arch/$(TARGET_MACHINE)$$;&;p' \
		-e '/\/arch\//d' \
		-e '/\/arch$$/d' \
		-e '/\/CVS$$/d' \
		-e '/\/kitchsrc\/tools/d' \
		-e '/\/kitchsrc\/doc/d' \
	    > gdbsrcdirs
	$(FIND) $(MKANCHOR)/$(TARGET_MACHINE)/$(OPTIMIZATION) -name '*.[CS]' -print \
	    | $(SED) -e 's/^/dir /' -e 's;/[^/]*$$;;' \
	    | $(SORT) -u \
	    >> gdbsrcdirs

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
