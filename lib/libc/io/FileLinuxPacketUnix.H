#ifndef __FILE_LINUX_PACKET_UNIX_H_
#define __FILE_LINUX_PACKET_UNIX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxPacketUnix.H,v 1.1 2005/04/21 19:22:49 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Unix domain 
 *                     datagram sockets.
 * **************************************************************************/

#include <io/FileLinuxPacket.H>
#include <sys/stat.h>
#include <fcntl.h>
#include <io/Socket.H>
#include <sys/uio.h>

class FileLinuxPacketUnix : public FileLinuxPacket {
public:
    DEFINE_GLOBAL_NEW(FileLinuxPacketUnix);
    FileLinuxPacketUnix(sval d, sval t, sval p): FileLinuxPacket(d, t, p) 
	{ /* empty body */ };
    virtual SysStatus bind(const char* addr, uval addrLen);
    virtual SysStatus connect(const char* addr, uval addrLen,
			      GenState &moreAvail, ThreadWait **tw);
};
#endif /* #ifndef __FILE_LINUX_PACKET_UNIX_H_ */
