# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.112 2005/08/16 14:02:41 dilma Exp $
# ############################################################################

default:: libobjs

# files that support ASI interface
COMASIC = PathName.C IO.C DirBuf.C MemTrans.C IORingBuffer.C
KERASIC = $(COMASIC)

USRASIC = $(COMASIC) IOBufferList.C DiskClient.C DiskClientAsync.C

ASIH    = $(USRASIC:%.C=%.H)

# files that support posix interface
COMPSXC = IOForkManager.C

KERPSXC = $(COMPSXC) IOForkManager.C

USRPSXC =	$(COMPSXC)			\
		FileLinux.C			\
		FileLinuxStream.C		\
		FileLinuxSocket.C		\
		FileLinuxPacket.C		\
		FileLinuxDir.C			\
		Buffer.C                	\
		FileLinuxFile.C			\
		FileLinuxDevNull.C		\
		FileLinuxDevZero.C		\
		FileLinuxDevRandom.C		\
		FileLinuxVirtFile.C		\
		FileLinuxSocketInet.C 	        \
		FileLinuxSocketUnix.C 	        \
		SysVMessagesClient.C		\
		FileLinuxPacketUnix.C


PSXH    = $(USRPSXC:%.C=%.H) \
	  NameTreeLinux.H FileLinuxClientStream.H FileLinuxStreamTTY.H \
	  CallBackObj.H

COMMINCS  = PathName.H $(ASIH) FileLinux.H Socket.H \
	    $(PSXH) FileLinuxServer.H VAPageServer.H \
	    PAPageServer.H StreamServer.H SocketServer.H \
	    IORingBuffer.H PacketServer.H \
	    GenState.H MemTrans.H BlockDev.H \
	    DiskClient.H DiskClientRO.H DiskClientAsync.H DiskClientAsyncRO.H \
	    iovec.H VirtFile.H SysVMessagesClient.H

CLASSES += StreamServer \
	   NameTreeLinux FileLinuxServer VAPageServer PAPageServer \
	   FileLinuxClientStream StreamServerTTY  MemTrans \
	   BlockDev CallBackObj \
	   VirtFile SysVMessagesClient

SERVE += PacketServer SocketServer

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

install_includes::
	$(INSTALL_INC) $(INCDIR)/io/         ./         $(COMMINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/..

COMMSRCS  = cprintf.C printfBuf.C
LIBCSRCS += $(COMMSRCS) $(USRASIC) $(USRPSXC) \
	    thinwire.C  IOForkManager.C
LIBKSRCS += $(COMMSRCS) $(KERASIC) $(KERPSXC) 
LIBSRVSRCS +=  XPacketServer.o XSocketServer.o StreamServer.C

CLASSES  +=
SRCS     +=
TARGETS  +=

# for kernel modules under the os/kernel tree
LIBKERNSRCS +=

# for server modules under the os/server tree
SERVERS     +=

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
