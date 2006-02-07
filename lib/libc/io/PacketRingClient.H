/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingClient.H,v 1.8 2002/10/10 13:08:17 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: client (user space) packet ring class
 ****************************************************************************/

#ifndef __PACKET_RING_CLIENT_H_
#define __PACKET_RING_CLIENT_H_

#include <cobj/CObjRoot.H>
#include <meta/MetaPacketRingClient.H>
#include <stub/StubPacketRingClient.H>
#include <stub/StubPacketRingServer.H>
#include <io/PacketRingBase.H>

class PacketRingClient;
typedef PacketRingClient **PacketRingClientRef;

// Prototype of callback function to be passed to processAllPackets
typedef void (*PacketCallback)(void *, uval, uval, uval);

// Prototype of callback to be called when packets are received
typedef void (*ReceiveCallback)(void *);

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class PacketRingClient : public PacketRingBase __xbase(Obj) {

protected:

    uval ringPtrTX;
    uval ringPtrRX;
    uval ctrlPagePtr;

#ifndef _IN_STUBGEN
    volatile uval32 *ctrlRegister;
#endif /* #ifndef _IN_STUBGEN */

    uval ringSizeTX;
    uval ringSizeRX;

    uval ringCurrentPtrTX;
    uval ringCurrentPtrRX;

    uval threadQueueRX;
    uval threadQueueTX;

    uval16 threadsBlockedRX;
    uval16 threadsBlockedTX;

    uval beginPtrTX;
    uval freeSpaceTX;
    uval packetCountTX;

    uval packetsReceiveNotify;
    uval packetsReceived;
    uval packetsReturned;
    uval packetsTransmitted;

    uval ticksPerSecond;

    BLock recvLock;
    BLock transLock;

    void (*recvCallback)(void *);
    void *recvCallbackArg;

    // Server side stub
    StubPacketRingServer stub;
    ObjectHandle stubOH;

    DEFINE_GLOBAL_NEW(PacketRingClient);

    virtual SysStatus init(PacketRingClientRef &pRef, ObjectHandle &prOH,
                           uval txSize, uval rxSize, uval txPtr, uval rxPtr);

    SysStatus registerCallback();
public:
    enum { DATA_RECV=0, DATA_SENT=1 };
    enum { NO_BLOCK=0, BLOCK=1 };

    struct iovec {
	uval iov_base;
	uval iov_len;
    };

    DEFINE_ACCESS_SERVER(
	PacketRingClient,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    // Constructor
    PacketRingClient() : stub(StubObj::UNINITIALIZED) { /* empty body */ }

    static SysStatus Create(PacketRingClientRef &pRef, uval txSize,
			    uval rxSize);

    virtual ObjectHandle &getOH();

    virtual SysStatus transmitPacket(iovec vec[], uval vecNum, uval flags);

    virtual SysStatus waitForPacket(sval64 timeout);

    virtual SysStatus getPacket(uval &vaddr, uval &size, uval &packetBase);

    virtual SysStatus releasePacket(uval vaddr, uval size);

    virtual SysStatus destroy();

    virtual SysStatus processAllPackets(PacketCallback fct, void *arg);

    virtual void registerRecvCallback(ReceiveCallback fct, void *arg);


private:

    void locked_reclaimSpaceTX();
    SysStatus locked_internalTransmitPacket(iovec vec[], uval vecNum,
                                            uval &usedSpace);
    SysStatus locked_getPacket(uval &vaddr, uval &size, uval &packetBase);

__xprivate:

    // Asynchronous notification methods
    virtual __async SysStatus _notifyDataReceive();
    virtual __async SysStatus _notifyDataTransmit();

};

#endif /* #ifndef __PACKET_RING_CLIENT_H_ */
