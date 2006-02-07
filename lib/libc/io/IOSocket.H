#ifndef __IOSOCKET_H_
#define __IOSOCKET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOSocket.H,v 1.42 2001/10/05 21:47:40 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FIXME: put info somewhere else and delete
 * **************************************************************************/

// FIXME: where does this belong
enum {INET =2};

class IOSocketAddr {
public:
    uval8 len;
    uval8 family;   // address family
    uval size();
};

// Socket address, internet style.
class IOSocketAddrIn : public IOSocketAddr {
public:
    uval16 port;
    uval32 addr;
    uval8 zero[8];

    IOSocketAddrIn() {family = INET;}
    IOSocketAddrIn(const struct sockaddr *sockaddr_in);
    IOSocketAddrIn(const char *inAddr, uval16 inPort);

    static uval32 InetAddr(const char *addr);
};
// FIXME: END where does this belong

class IOSocket {
public:

    //NOTE  - Linux headers can't be included in IOSocket,
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

#endif /* #ifndef __IOSOCKET_H_ */
