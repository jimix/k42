# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.109 2005/06/28 19:44:38 rosnbrg Exp $
# ############################################################################

SUBDIRS =

PERCS_SUBDIR=percs

SUBDIRS += $($(K42_EXTRAS:%=%_SUBDIR))

# set KERNEL=1 if for kernel
KERNEL=1

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

#libobjs :: subdirs_libobjs
#install_libobjs :: subdirs_install_libobjs

COMMINCS+= ComPort.H IOChan.H HVChan.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/sys/            ./            $(COMMINCS)



ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

SERVE += Wire TestSwitch TestScheduler \
	 KernSimDisk KernToyDisk KernBogusDisk NetDev \
         PrivilegedService SystemMisc FSFRSwap DiskSwap IPSock KBootParms \
	 BuildDate

LIBKERNSRCS += libksup.C thinwire.C Wire.C ThinIP.C \
	resourcePrint.C  ThinWireMgr.C \
	SocketWire.C FSRamSwap.C FSSwap.C FSFRSwap.C DiskSwap.C TestSwitch.C \
	TestScheduler.C \
        KernToyDisk.C KernBogusDisk.C ToyBlockDev.C \
        SystemMisc.C PrivilegedService.C \
	LocalConsole.C StreamServerConsole.C NetDev.C \
	BlockDevBase.C COSMgrObjectKernObject.C LazyState.C SystemControl.C \
	MIPKernelMethods.C KParms.C KBootParms.C BuildDate.C SysEnviron.C \
	ZilogPort.C UARTPort.C HVChan.C \
	$(SERVE:%=X%.C)


LINKDATE:='"$(shell date)"'
MAKEUSER:='"$(USER)"'
REGRESSTS:='"$(shell ls -l $(MKANCHOR)/kitchsrc/RegressStamp|\
	      awk '{printf "%s %s %s",$$6,$$7,$$8}' )"'
DBGLVL='"nodebug"'
ifeq ($(OBJDIR_OPT_TYPES),fullDeb)
DBGLVL='"fulldebug"'
endif
ifeq ($(OBJDIR_OPT_TYPES),partDeb)
DBGLVL='"partdebug"'
endif

.PHONY: FORCE

BuildDate.o: FORCE

BuildDate_FLAGS+=-DMAKEUSER=$(MAKEUSER) \
	-DLINKDATE=$(LINKDATE) -DREGRESSTS=$(REGRESSTS) \
	-DDBGLVL=$(DBGLVL)

ifeq ($(TARGET_MACHINE),mips64)
SocketWire.o: SocketWire.C
	$(TARGET_CC) -c $(TARGET_CFLAGS) $(CPPFLAGS) -g0 -DDWARF_HACK $< -o $@
endif

ifneq (,$(findstring PERCS,$(K42_EXTRAS)))
priv_VPATH=$(SRC_DIR_PATH)/$(PERCS_SUBDIR):$(SRC_DIR_PATH)/arch/$(TARGET_MACHINE)/private:
else
priv_VPATH=$(SRC_DIR_PATH)/arch/$(TARGET_MACHINE)/private:
endif

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
