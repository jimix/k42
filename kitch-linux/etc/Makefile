# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.21 2005/07/28 17:59:09 dilma Exp $
# ############################################################################
# ############################################################################
# These file originated from the setup-2.1.8 rpm. They have no copyright!
# ############################################################################

# ############################################################################
# WARNING: This Makefile has some serious stuff in it.. Be Carefull!!
# ############################################################################


SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR

SETUP =	bashrc		\
	csh.cshrc	\
	csh.login	\
	exports		\
	group		\
	host.conf	\
	hosts		\
	hosts.allow	\
	hosts.deny	\
	inputrc		\
	motd		\
	nsswitch.conf	\
	passwd		\
	printcap	\
	profile		\
	protocols	\
	resolv.conf	\
	services

ifeq "$(K42_PKGVER)" "3"
SETUP += localtime filesystems
endif

GENERATE = ld.so.conf groff terminfo

#		securetty	#  install as mode 600

# this target may need to be install in the above directory because of parallel
# build reasons.
# profile.d is where other packages place there init shell scripts.
dir_SETUP =   profile.d

ETC_DIR=$(MKKITCHROOT)/etc


# The default VPATH generates circular dependecies when src and result
# are the same name but different directories.
VPATH=

#
# The following lines solve the problem of users being able to
# customize the installed configuration of the system.
#
#
# Basically, thes configureation files can be obtained from 3
# directories (in order of preference):
#
#   $(K42_USER_CONFIG):  Files specific to user.
#   $(K42_CONFIG):       System wide information for a group of users.
#   Source Dir:          Which contains the bare minimum information for
#		         these files.
#
# If any of these directories or files do not exist then make should
# not fail, but continue on.
#

# First find those targets defined in the user's K42_USER_CONFIG directory
# (usually "~/k42-config").
usr_SETUP=$(foreach t, $(SETUP), $(notdir $(wildcard $(K42_USER_CONFIG)/$(t))))

# Then find those targets defined in the group's K42_CONFIG directory (usually
# "/u/kitchawa/k42-config") excluding the ones that we found in the
# users directory
ifneq ($(K42_CONFIG),)
sys_SETUP=$(filter-out $(usr_SETUP), \
	$(foreach t, $(SETUP), $(notdir $(wildcard $(K42_CONFIG)/$(t)))))
else
sys_SETUP=
endif

# Finally, find those targets in this directory excluding the ones from above.
def_SETUP=$(filter-out $(usr_SETUP) $(sys_SETUP), $(SETUP))

$(usr_SETUP:%=$(ETC_DIR)/%): $(ETC_DIR)/%: $(K42_USER_CONFIG)/% Makefile
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)


$(sys_SETUP:%=$(ETC_DIR)/%): $(ETC_DIR)/%: $(K42_CONFIG)/% Makefile
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)

$(def_SETUP:%=$(ETC_DIR)/%): $(ETC_DIR)/%: $(SRC_DIR_PATH)/% Makefile
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)

ld.so.conf: Makefile
	echo "/usr/lib /klib" >ld.so.conf

$(ETC_DIR)/groff:
	ln -sf /nfs/etc/groff $@

$(ETC_DIR)/terminfo:
	ln -sf /nfs/etc/terminfo $@

$(ETC_DIR)/ld.so.conf: ld.so.conf 
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)


install_targets:: $(SETUP:%=$(ETC_DIR)/%) $(GENERATE:%=$(ETC_DIR)/%)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
