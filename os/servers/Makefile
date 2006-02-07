# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.93 2005/06/28 19:47:47 rosnbrg Exp $
# ############################################################################

#
# NOTE: Targets included in baseServers are built in the baseServers dir
#

SUBDIRS = shell loadImage traced sample userProc ois

# following subdirectories only needed for local debugging, made in baseservers
# SUBDIRS += nfs pty pipe dev mountPoint ext2
SUBDIRS += procfs unionfs


# these are needed for stub objects
SUBDIRS_STUB = mountPoint pty pipe nfs k42Ramfs


#Following needed to check for definition of FILESYS
include $(MKSRCTOP)/Make.paths

ifeq (KFS, $(FILESYS))
ifndef KFS_UNSTABLE
SUBDIRS_STUB += kfs
else
SUBDIRS_STUB += kfs-unstable
endif
endif

# sub-directories containing memebers of bootServers
SUBDIRS_BOOT = baseServers reboot

ifeq ($(filter bootServers% %targets ,$(MAKECMDGOALS)),)
# go into these target for everything but targets and install_targets
SUBDIRS += $(SUBDIRS_BOOT)
SUBDIRS += $(SUBDIRS_STUB)
endif

default:: targets

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
targets :: subdirs_targets
install_targets :: subdirs_install_targets

TARGETS += bootServers.o

SUBDIRS_SERVER_LISTS = $(SUBDIRS_BOOT:%=%/servers.list)


# This how you force a target to always run
# Any target that depends on a target with no dependecy will force the
# original target.
.PHONY: forced_entry
forced_entry:

$(SUBDIRS_SERVER_LISTS): forced_entry
	$(MAKE) -C $(@D) $(MAKECMDGOALS)

bootServers.s: $(SUBDIRS_SERVER_LISTS)
	$(MKSERV) $(SUBDIRS_SERVER_LISTS:%=-F %) -o $@

bootServers.o: bootServers.s
	$(TARGET_CC) -c $(TARGET_CFLAGS) $(ASFLAGS) $(CPPFLAGS) $< -o $@

clean ::
	-$(RM) bootServers.s bootServers.d


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
