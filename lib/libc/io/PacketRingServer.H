#ifndef __PACKET_RING_SERVER_H_
#define __PACKET_RING_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingServer.H,v 1.2 2001/10/05 21:47:42 peterson Exp $
 *****************************************************************************/
#include <io/PacketRingBase.H>

class PacketRingServer;
typedef PacketRingServer** PacketRingServerRef;

class PacketRingServer : public PacketRingBase __xbase(Obj) {

public:

    static void ClassInit(VPNum vp);

    virtual SysStatus bindFilter()=0;
    virtual SysStatus unbindFilter()=0;
    virtual SysStatus getType(TypeID &id);

    virtual void receivedPacket(char *packet, uval length)=0;
    virtual void transmittedPacket()=0;

__xprivate:
    static SysStatus _Create(__in uval txSize, __in uval rxSize,
			     __out uval &vaddrTX, __out uval &vaddrRX,
			     __out ObjectHandle &prOH, __CALLER_PID caller);

    virtual SysStatus _registerCallbackObj(__in ObjectHandle callback,
					   __XHANDLE xhandle)=0;

    virtual SysStatus _transmit()=0;

    virtual SysStatus _destroy()=0;
};

#endif /* #ifndef __PACKET_RING_SERVER_H_ */


