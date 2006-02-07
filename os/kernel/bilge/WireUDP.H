#ifndef __WIRE_UDP_H_
#define __WIRE_UDP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: WireUDP.H,v 1.2 2001/10/05 21:49:11 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a UDP socket interface using thinwire.
 *
 * As temporary cludge, not yet tying into user level IO derivatoin tree.
 * **************************************************************************/

#include <bilge/SocketWire.H>

#define INSTNAME SocketWire
#include <meta/TplMetaPacketServer.H>
#include <xobj/TplXPacketServer.H>

class WireUDP;
typedef WireUDP **WireUDPRef;
typedef TplXPacketServer<SocketWire>  XWireUDPServer;
typedef TplMetaPacketServer<SocketWire>  MetaWireUDPServer;

class WireUDP : public SocketWire {
public:
    DEFINE_GLOBAL_NEW(WireUDP);
    DEFINE_ACCESS_SERVER(WireUDPServer,
			 MetaObj::write|MetaObj::read|MetaObj::controlAccess,
			 MetaObj::none);
};

#endif /* #ifndef __WIRE_UDP_H_ */
