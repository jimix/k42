#ifndef __FILE_LINUX_SOCKET_UNIX_H_
#define __FILE_LINUX_SOCKET_UNIX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxSocketUnix.H,v 1.4 2005/04/19 02:27:06 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality for
 *                     sockets.
 * **************************************************************************/

#include <io/FileLinuxSocket.H>
#include <sys/socket.h>

class FileLinuxSocketUnix : public FileLinuxSocket {
public:
    DEFINE_GLOBAL_NEW(FileLinuxSocketUnix);

    FileLinuxSocketUnix(sval d, sval t, sval p) : FileLinuxSocket(d, t, p)
        { /* empty body */ };

    static SysStatus Create(FileLinuxRef& newSocket, sval domain, sval type,
			    sval protocol);

    virtual SysStatus bind(const char* saddr, uval addrLen);
    virtual SysStatus connect(const char* addr, uval addrLen,
			      GenState &moreAvail, ThreadWait **tw);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);
};


#endif /* #ifndef __FILE_LINUX_SOCKET_UNIX_H_ */
