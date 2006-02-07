#ifndef __WIRE_TCP_H_
#define __WIRE_TCP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: WireTCP.H,v 1.4 2001/11/13 16:01:37 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a TCP socket interface using thinwire.
 *
 * As temporary cludge, not yet tying into user level IO derivatoin tree.
 * **************************************************************************/

#include <bilge/SocketWire.H>

#include <meta/TplMetaSocketServer.H>
#include <xobj/TplXSocketServer.H>

class WireTCP;
typedef WireTCP **WireTCPRef;
typedef TplXSocketServer<SocketWire>  XWireTCPServer;
typedef TplMetaSocketServer<SocketWire>  MetaWireTCPServer;

class WireTCP : public SocketWire {
public:
    DEFINE_GLOBAL_NEW(WireTCP);

    DEFINE_ACCESS_SERVER(WireTCPServer,
			 MetaObj::write|MetaObj::read|MetaObj::controlAccess,
			 MetaObj::none);
    virtual SysStatus _listen(__in sval backlog);
    virtual SysStatus _accept(__out ObjectHandle &oh,
			      __inoutbuf(len:len:len) char* addr,
			      __inout uval& len,
			      __out GenState &moreAvail,
			      __XHANDLE xhandle);
};

#endif /* #ifndef __WIRE_TCP_H_ */
