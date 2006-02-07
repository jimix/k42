#ifndef __IP_SOCK_H_
#define __IP_SOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IPSock.H,v 1.2 2003/10/14 17:56:06 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using the linux TCP/IP stack.
 * **************************************************************************/

#include <io/SocketServer.H>
#include <meta/MetaSocketServer.H>
#include <xobj/XSocketServer.H>
#include <misc/ListSimple.H>
#include <io/FileLinux.H>
#include <io/SocketServer.H>

class IPSock;
typedef IPSock** IPSockRef;
class IPSock : public SocketServer {
public:
    static void ClassInit();
__xpublic:
    /* exported functions */
    static SysStatus _Create(__out ObjectHandle &oh, __out uval &clientType,
			     __in uval domain, __in uval type,
			     __in uval protocol, __CALLER_PID processID);
};

#endif /* #ifndef __IP_SOCK_H_ */
