#ifndef __PACKET_SERVER_H_
#define __PACKET_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketServer.H,v 1.11 2005/05/09 18:33:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interfaces for network data sources.
 * **************************************************************************/
#ifndef EXPORT_PACKETSERVER
#define EXPORT_PACKETSERVER
#include <io/GenState.H>
#include <sys/types.h>
#include <sys/ProcessLinux.H>
#endif /* #ifndef EXPORT_PACKETSERVER */
#include <io/StreamServer.H>


class PacketServer : public StreamServer {
public:
    static void ClassInit(VPNum vp);
    DEFINE_GLOBAL_NEW(PacketServer);
__xpublic:
    virtual SysStatus _connect(__inbuf(addrLen) const char *addr,
			       __in uval addrLen,
			       __out GenState &moreAvail,
			       __XHANDLE xhandle) = 0;

    //
    // Bind call must promise never to change length of addr buffer
    //
    virtual SysStatus _bind(__inoutbuf(addrLen:addrLen:addrLen) char *addr,
			    __inout uval &addrLen, __XHANDLE xhandle) = 0;

    virtual SysStatus _setsockopt(__in uval level, __in uval optname,
				  __inbuf(optlen) char *optval,
				  __in uval optlen, __XHANDLE xhandle) = 0;
    virtual SysStatus _getsockopt(__in uval level, __in uval optname,
				  __outbuf(optlen:optlen) char *optval,
				  __inout uval *optlen, __XHANDLE xhandle) = 0;
};

#endif /* #ifndef __PACKET_SERVER_H_ */
