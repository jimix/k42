/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRing.H,v 1.5 2002/10/10 13:08:32 rosnbrg Exp $
 *****************************************************************************/
#ifndef __PACKET_RING_H_
#define __PACKET_RING_H_

#include <cobj/CObjRoot.H>
#include <io/PacketRingServer.H>
#include <stub/StubPacketRingServer.H>
#include <meta/MetaPacketRingServer.H>
#include <stub/StubPacketRingClient.H>
#include <sys/macdefs.H>

class PacketRing;
typedef PacketRing** PacketRingRef;

class PacketRing : public PacketRingServer {

public:
    enum { MAX_PAGES = (65536 / PAGE_SIZE) };

    ProcessRef processRef;
    RegionRef regionRef;
    FCMRef fcmRef;
    PacketRingRef packetRingRef;

    StubPacketRingClient stub;

    uval baseVaddr;
    uval fcmOffset;
    uval pageArrayNumTX;
    uval pageArrayNumRX;
    uval ctrlPage;
    uval pageArray[MAX_PAGES*2];

    volatile uval32 *ctrlRegister;

    uval ringCurrentIdxRX;
    uval ringCurrentOffsetRX;
    uval ringCurrentIdxTX;
    uval ringCurrentOffsetTX;

    uval beginIdxTX;
    uval beginOffsetTX;

    uval beginIdxRX;
    uval beginOffsetRX;

    uval freeSpaceRX;
    uval packetCountRX;

    sval refCount;
    uval beingDestroyed;

    uval packetsReceived;
    uval packetsTransmitted;
    uval packetsDropped;
    uval packetsRecvCoalesced;

    DEFINE_GLOBAL_NEW(PacketRing);

    DEFINE_ACCESS_SERVER(
    	PacketRingServer,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    PacketRing() : stub(StubObj::UNINITIALIZED) 
    {
      /* empty body */
    }

    virtual SysStatus init(ProcessID caller, uval &vaddrTX, uval &vaddrRX,
                           uval txSize, uval rxSize);
    virtual SysStatus exportedXObjectListEmpty();
    virtual SysStatus destroy();

public:

    static void ClassInit(VPNum vp);

    static SysStatus InternalCreate(PacketRingRef &pR, uval txSize,
				    uval rxSize, uval &vaddrTX, uval &vaddrRX,
				    ProcessID caller);

    virtual void receivedPacket(char *packet, uval length);
    virtual void transmittedPacket();

    virtual SysStatus bindFilter();
    virtual SysStatus unbindFilter();

    virtual SysStatus _registerCallbackObj(__in ObjectHandle callback,
                                           __XHANDLE xhandle);

    virtual SysStatus _transmit();

    virtual SysStatus _destroy();

private:
    SysStatus allocRings(uval txSize, uval rxSize);

    SysStatus allocCtrl();

    void reclaimSpaceRX();
};

#endif /* #ifndef __PACKET_RING_H_ */
