#ifndef __SOCKET_H_
#define __SOCKET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Socket.H,v 1.44 2001/11/01 19:54:06 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Socket address struct definitions
 * **************************************************************************/
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Socket address, internet style.
class SocketAddrIn : public sockaddr_in {
public:
    SocketAddrIn(uval32 addr = 0, uval16 port = 0) {
	sin_family = AF_INET;
	sin_port = port;
	sin_addr.s_addr = addr;
    }
};

class Socket {
public:

    //NOTE  - Linux headers can't be included in Socket,
    //        because of the conflicts it causes with
    //        SocketNetBSD. Therefore, presently
    //        a K42 type canonical "type" is chosen and
    //        all external "type"s are converted.

    // defined type values
    enum {
	STREAM = 1,
        DATAGRAM = 2,
	RAW = 3
    };
};

#endif /* #ifndef __SOCKET_H_ */
