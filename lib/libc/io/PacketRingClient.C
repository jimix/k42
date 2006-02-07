/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingClient.C,v 1.8 2001/02/06 16:12:37 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: client (user space) packet ring class
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRoot.H>
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <sync/BlockedThreadQueues.H>
#include <sync/atomic.h>
#include <stub/StubPacketRingServer.H>
#include <meta/MetaPacketRingServer.H>
#include <io/PacketRingClient.H>

#if 0
#undef cprintf
#define cprintf(args...)
#endif


/* virtual */ SysStatus
PacketRingClient::init(PacketRingClientRef &pRef, ObjectHandle & prOH,
                       uval txSize, uval rxSize, uval txPtr, uval rxPtr)
		       
{
    ringPtrTX = txPtr;
    ringPtrRX = rxPtr;

    ringSizeTX = txSize;
    ringSizeRX = rxSize;

    ringCurrentPtrTX = ringPtrTX;
    ringCurrentPtrRX = ringPtrRX;

    ctrlPagePtr = ringPtrTX + PAGE_ROUND_UP(txSize);
    ctrlRegister = (volatile uval32 *)ctrlPagePtr;
    
    //AtomicOr32Synced(ctrlRegister, PacketRingServer::RECV_INTR_MASK);
    //AtomicOr32Synced(ctrlRegister, PacketRingServer::TRANS_INTR_MASK);

    threadsBlockedRX = 0;
    threadsBlockedTX = 0;

    beginPtrTX = ringPtrTX;
    freeSpaceTX = txSize - sizeof(PacketRingHdr);
    packetCountTX = 0;

    packetsReceiveNotify = 0;
    packetsReceived = 0;
    packetsTransmitted = 0;
    packetsReturned = 0;

    recvCallback = NULL;
    recvCallbackArg = NULL;

    ticksPerSecond = Scheduler::TicksPerSecond();

    recvLock.init();
    transLock.init();

    stubOH = prOH;
    stub.setOH(prOH);

    pRef = (PacketRingClientRef)CObjRootSingleRep::Create(this);

    cprintf("PacketRingClient::init - Base: Recv %#lx, Trns %#lx, Ctrl %#lx\n",
            ringCurrentPtrRX, ringCurrentPtrTX, ctrlPagePtr);
    cprintf("PacketRingClient::init - End: Recv: %#lx, Trans: %#lx\n",
            ringCurrentPtrRX + rxSize, ringCurrentPtrTX + txSize);
    cprintf("PacketRingClient::init - ticksPerSecond: %ld\n", ticksPerSecond);

    registerCallback();

    return 0;
}


/* static */ SysStatus
PacketRingClient::Create(PacketRingClientRef &pRef, uval txSize, uval rxSize)
{
    PacketRingClient *newp;
    ObjectHandle tmpOH, prOH;
    uval txPtr, rxPtr;
    SysStatus rc;
    
    cprintf("PacketRingClient::Create\n");

    txSize = PAGE_ROUND_UP(txSize);
    rxSize = PAGE_ROUND_UP(rxSize);

    rc = StubPacketRingServer::_Create(txSize, rxSize, txPtr, rxPtr, prOH);
    if (_SUCCESS(rc)) {
	cprintf("PacketRingClient::Create - Server created\n");
	newp = new PacketRingClient();
	newp->init(pRef, prOH, txSize, rxSize, txPtr, rxPtr);
	tassert(_SUCCESS(rc), err_printf("register callback failed\n"));
    } else {
	cprintf("PacketRingClient::Create - Server create failed\n");
    }
    
    return rc;
}

SysStatus
PacketRingClient::registerCallback()
{
    ObjectHandle tmpOH;
    SysStatus rc;

    rc = giveAccessByServer(tmpOH, stub.getPid());
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    return stub._registerCallback(tmpOH);
}

/* virtual */ SysStatus
PacketRingClient::destroy()
{
    AutoLock<BLock> al(&recvLock);
    AutoLock<BLock> al2(&transLock);

    return stub._destroy();
}

/* virtual */ ObjectHandle &
PacketRingClient::getOH()
{
  return stubOH;
}

/* virtual */ void
PacketRingClient::registerRecvCallback(ReceiveCallback callback, void *arg)
{
    recvCallbackArg = arg;
    recvCallback = callback;

    return;
}

/* 
 * waitForPacket - will block until data is ready to be read from the packet
 * ring - timeout is relative in microseconds, if -1 then no timeout
 */
/* virtual */ SysStatus
PacketRingClient::waitForPacket(sval64 timeout)
{
    BlockedThreadQueues::Element q;
    SysTime timeoutTicks=0;
    uval timedOut=1;
    
    recvLock.acquire(); // Lock

    cprintf("PacketRingClient::waitForPackets: Hdr=%#lx, idx=%ld, off=%ld\n",
            ringCurrentPtrRX, (ringCurrentPtrRX - ringPtrRX) / PAGE_SIZE, 
            (ringCurrentPtrRX - ringPtrRX) % PAGE_SIZE);

    cprintf("PacketRingClient::waitForPackets seen: %ld, got: %ld, ret: %ld\n",
            packetsReceiveNotify, packetsReceived, packetsReturned);

    // Convert timeout from ms to ticks
    if (timeout > 0) {
        timeoutTicks = MAX((ticksPerSecond * timeout) / 1000000, 1);
        timeoutTicks += Scheduler::SysTimeNow();
        timedOut = 0;
        cprintf("PacketRingClient::waitForPackets timeout: %lld, ticks: %lld\n",
                timeout, timeoutTicks);
    } else if (timeout == -1) {
      timedOut = 0;
    }

    // Check for data    
    while ((!(RingHdrValid(ringCurrentPtrRX))) && (timedOut == 0)) {

        cprintf("PacketRingClient::waitForPacket - timeout: %lld time: %lld\n",
                timeoutTicks, Scheduler::SysTimeNow());

        cprintf("PacketRingClient::waitForPacket - blocking\n");

	// Block current thread
	threadsBlockedRX = 1;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(&q, 
                                                      (void *)&threadQueueRX);
        recvLock.release(); // Unlock

        // Depending on timeout call correct block method
        if (timeout == -1) {
          Scheduler::Block();
        } else {
          Scheduler::BlockWithTimeout(timeoutTicks, TimerEvent::absolute);
        }

	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(&q, 
                                                      (void *)&threadQueueRX);
        recvLock.acquire();  // Don't like this...

        cprintf("PacketRingClient::waitForPackets - checking... " 
                "seen: %ld, got: %ld, ret: %ld\n",
                packetsReceiveNotify, packetsReceived, packetsReturned);

        // Check for a timeout
        if (Scheduler::SysTimeNow() >= timeoutTicks && timeout != -1) {
            timedOut = 1;
            cprintf("PacketRingClient::waitForPacket - possible timeout\n");
        }
    }
    
    cprintf("PacketRingClient::waitForPacket - returning\n");

    recvLock.release();

    return 0;
}

/* virtual */ SysStatus
PacketRingClient::processAllPackets(PacketCallback callback, void *arg)
{
    uval vaddr, size, packetBase;
    SysStatus rc;

    if (callback == NULL) {
        return -1;
    }

    cprintf("PacketRingClient::processAllPackets\n");

    do {
        *ctrlRegister |= CHECKING_READ; // Set bit

        recvLock.acquire();
        rc = locked_getPacket(vaddr, size, packetBase);
        recvLock.release();
        
        if (!_FAILURE(rc)) {
            cprintf("PacketRingClient::processAllPackets - found packet\n");
            callback(arg, vaddr, size, packetBase);
        }

        *ctrlRegister &= ~CHECKING_READ; // Clear bit

    } while (RingHdrValid(ringCurrentPtrRX));

    cprintf("PacketRingClient::processAllPackets - Done\n");

    return 0;
}

/* virtual */ SysStatus
PacketRingClient::getPacket(uval &vaddr, uval &size, uval &packetBase)
{
    AutoLock<BLock> al(&recvLock);
    SysStatus rc;

    *ctrlRegister |= CHECKING_READ; // Set bit

    rc = locked_getPacket(vaddr, size, packetBase);
        
    // FIXME: this is unsafe -- as all packets may have not been consumed
    *ctrlRegister &= ~CHECKING_READ; // Clear bit

    return rc;
}


/* 
 * getPacket - reads a packet from the packet ring, returns failure if there
 * is no packet to read.  It never blocks.
 */
/* private */ SysStatus
PacketRingClient::locked_getPacket(uval &vaddr, uval &size, uval &packetBase)
{
    PacketRingHdr *recvHdr;

    cprintf("PacketRingClient::getPacket: Hdr=%#lx, idx=%ld, off=%ld\n",
            ringCurrentPtrRX, (ringCurrentPtrRX - ringPtrRX) / PAGE_SIZE, 
            (ringCurrentPtrRX - ringPtrRX) % PAGE_SIZE);

    cprintf("PacketRingClient::getPacket: seen: %ld, got: %ld, ret: %ld\n",
            packetsReceiveNotify, packetsReceived, packetsReturned);

    recvHdr = (PacketRingHdr *)(ringCurrentPtrRX);

    // Check for data    
    if (!RingHdrValid(ringCurrentPtrRX)) {
	size = 0;
        cprintf("PacketRingClient::getPacket - no packets found\n");
        recvLock.release();

	return _SERROR(1648, 0, EINVAL);
    }

    // Set base addr of packet in ring - this should be used in releasePacket
    packetBase = ringCurrentPtrRX;

    // Set addr of current data
    vaddr = ringPtrRX + ((ringCurrentPtrRX + recvHdr->prepad + 
                          sizeof(PacketRingHdr) - ringPtrRX) % (ringSizeRX));

    tassert(vaddr < ringPtrRX + ringSizeRX, 
            err_printf("vaddr out of bounds\n"));

    // Set size of current data
    size = recvHdr->length;

    // Increment current ptr    
    ringCurrentPtrRX = ringPtrRX + ((recvHdr->prepad + sizeof(PacketRingHdr) + 
                                     recvHdr->postpad + recvHdr->length + 
                                     ringCurrentPtrRX - ringPtrRX) % 
                                    ringSizeRX);

    tassert(ringCurrentPtrRX < ringPtrRX + ringSizeRX, 
            err_printf("ringCurrentPtrRX out of bounds\n"));

    packetsReceived++;
    
    cprintf("PacketRingClient::getPacket - addr: %#lx, sz: %ld, total=%ld\n", 
            vaddr, size, packetsReceived);

    cprintf("PacketRingClient::getPacket: Hdr=%#lx, idx=%ld, off=%ld\n",
            ringCurrentPtrRX, (ringCurrentPtrRX - ringPtrRX) / PAGE_SIZE, 
            (ringCurrentPtrRX - ringPtrRX) % PAGE_SIZE);

    return 0;
}

/* virtual */ SysStatus
PacketRingClient::releasePacket(uval packetBase, uval size)
{
    AutoLock<BLock> al(&recvLock);

    cprintf("PacketRingClient::releasePacket: %#lx\n", packetBase);
    
    // Check that it was valid (this is just a sanity check, there is no
    // real protection from the app passing in bad addresses -- we could
    // keep a hash table or something but hey if the app wants to hurt itself..
    if (!(RingHdrValid(packetBase))) {
        cprintf("PacketRingClient::releasePacket - found invalid hdr\n");
        tassert(0, err_printf("Invalid packet released\n"));
        return _SERROR(1649, 0, EINVAL);
    }

    RingHdrInvalidate(packetBase);  // Set flag to invalid/clean

    packetsReturned++;

    cprintf("PacketRingClient::releasePacket - returned=%ld, gotten=%ld\n",
            packetsReturned, packetsReceived);

    return 0;
}


/* virtual */ SysStatus
PacketRingClient::transmitPacket(iovec vec[], uval vecNum, uval flags)
{
    SysStatus rc;
    BlockedThreadQueues::Element q;
    uval usedSpace;

    transLock.acquire();  // Lock

    // Do transmit 
    rc = locked_internalTransmitPacket(vec, vecNum, usedSpace);

    while (_FAILURE(rc) && flags != NO_BLOCK) {

        // Clear trans intr mask
        AtomicAnd32Volatile(ctrlRegister, ~TRANS_INTR_MASK);

        // Block waiting for freeSpace
        while (freeSpaceTX < usedSpace) {
            threadsBlockedTX = 1;
            DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(&q, 
                                                       (void *)&threadQueueTX);
            transLock.release(); // Unlock

            cprintf("PacketRingClient::transmitPacket - blocking (rc=%ld)\n",
                    rc);
            Scheduler::Block();
            DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
                                                                          &q,
                                                       (void *)&threadQueueTX);
            transLock.acquire(); // Lock

        }     

        // Set trans intr mask
        AtomicOr32Volatile(ctrlRegister, TRANS_INTR_MASK);
        
        rc = locked_internalTransmitPacket(vec, vecNum, usedSpace);
    }

    transLock.release(); // Unlock

    return rc;
}

SysStatus
PacketRingClient::locked_internalTransmitPacket(iovec vec[], uval vecNum, 
                                                uval &usedSpace)
{
    SysStatus rc;
    uval nextHdrPtr, postpad=0, prepad=0, tmp;
    uval oldHdrPtr;

    uval nextHdrIdx, nextHdrOffset;
    uval ringCurrentOffsetTX, ringCurrentIdxTX, tmpPtrTX;
    uval numPages = (PAGE_ROUND_UP(ringSizeTX)) >> LOG_PAGE_SIZE;
    uval i, length=0, offset=0;

    for(i = 0; i < vecNum; i++) {
      length += vec[i].iov_len;
    }

    cprintf("PacketRingClient::transmitPacket - base=%#lx, curr=%#lx\n",
            ringPtrTX, ringCurrentPtrTX);

    // Set hdr ptr for this packet
    oldHdrPtr = ringCurrentPtrTX;

    // Save state and incr current position in ring
    tmpPtrTX = ringCurrentPtrTX + sizeof(PacketRingHdr);
    if (tmpPtrTX >= (ringPtrTX + ringSizeTX)) {
        tmpPtrTX = (ringPtrTX + ((tmpPtrTX - ringPtrTX) % ringSizeTX));
    }

    ringCurrentIdxTX = (tmpPtrTX - ringPtrTX) >> LOG_PAGE_SIZE;
    ringCurrentOffsetTX = (tmpPtrTX - ringPtrTX) % PAGE_SIZE;

    /* First figure out where next packet is going to go 
     * Make sure ring hdrs are aligned and do not overlap pages 
     * This only works if hdr size is a multiple of the alignment size
     */
    nextHdrIdx = ringCurrentIdxTX;
    tmp = ALIGN_UP(length + ringCurrentOffsetTX, 8);
    nextHdrOffset = tmp % PAGE_SIZE;
    postpad = tmp - (length + ringCurrentOffsetTX);

    if ((tmp + sizeof(PacketRingHdr)) > PAGE_SIZE) {
        nextHdrIdx++;

        if (tmp <= PAGE_SIZE) {
            nextHdrOffset = 0;
            postpad += PAGE_SIZE - tmp;
        }

        /* Don't want data to wrap around ring edge */
        if (nextHdrIdx >= numPages && nextHdrOffset != 0) {
            nextHdrOffset = ALIGN_UP(length, 8);
            nextHdrIdx = 0;
            prepad = PAGE_SIZE - ringCurrentOffsetTX;
            postpad = nextHdrOffset - length;
        }

        nextHdrIdx = nextHdrIdx % numPages;
    }

    // Calculate space taken by this packet
    usedSpace = prepad + length + postpad + sizeof(PacketRingHdr);
    cprintf("PacketRingClient::transmitPacket - "
            "(pre=%ld, post=%ld, idx=%ld, off=%ld, used=%ld, free=%ld)\n", 
            prepad, postpad, ringCurrentIdxTX, ringCurrentOffsetTX, usedSpace,
            freeSpaceTX);

    // Try and reclaim space if possible 
    if (usedSpace > freeSpaceTX) {
        locked_reclaimSpaceTX();
    }
     
    // Fail if there is no free space, blocking is done outside this func.
    if (usedSpace > freeSpaceTX) {
        cprintf("PacketRingClient::transmitPacket - no space: %ld/%ld\n", 
                usedSpace, freeSpaceTX);
        return _SERROR(1650, 0, EINVAL);
    }

    freeSpaceTX -= usedSpace;
    packetCountTX++;

    // Restore state
    ringCurrentPtrTX = tmpPtrTX;

    // Zero out next header
    nextHdrPtr = (nextHdrIdx << LOG_PAGE_SIZE) + nextHdrOffset + ringPtrTX;  
    memset((void *)nextHdrPtr, 0, sizeof(PacketRingHdr));

    // Copy packet into ring
    ringCurrentPtrTX = (ringPtrTX +
                        ((ringCurrentPtrTX+prepad-ringPtrTX)%(ringSizeTX)));
    offset = 0;
    for (i = 0; i < vecNum; i++) {
      cprintf("PacketRingClient::transmitPacket - dest: %#lx, len: %ld\n",
              ringCurrentPtrTX + offset, vec[i].iov_len);
      tassert((ringCurrentPtrTX + offset + vec[i].iov_len) < 
              (ringPtrTX + ringSizeTX), 
              err_printf("ringCurrentPtrTX out of bounds\n"));
      memcpy((void *)(ringCurrentPtrTX + offset), (void *)vec[i].iov_base, 
             vec[i].iov_len);
      offset += vec[i].iov_len;
    }

    ringCurrentPtrTX = nextHdrPtr;

    tassert(ringCurrentPtrTX < ringPtrTX + ringSizeTX, 
            err_printf("ringCurrentPtrTX out of bounds\n"));

    // Set this header
    ((PacketRingHdr *)(oldHdrPtr))->prepad = prepad;
    ((PacketRingHdr *)(oldHdrPtr))->postpad = postpad;
    ((PacketRingHdr *)(oldHdrPtr))->length = length;

    RingHdrValidate(oldHdrPtr);

    packetsTransmitted++;

    cprintf("PacketRingClient::transmitPacket - call _transmit (total=%ld)\n",
            packetsTransmitted);

    rc = stub._transmit();

    return rc;
}


void
PacketRingClient::locked_reclaimSpaceTX()
{
    PacketRingHdr *trans;
    uval length;

    cprintf("PacketRingClient::reclaimSpaceTX - (free=%ld, count=%ld)\n",
            freeSpaceTX, packetCountTX);
  
    while(!(RingHdrValid(beginPtrTX)) && (packetCountTX > 0)) {

        trans = (PacketRingHdr *)beginPtrTX;

	length = (sizeof(PacketRingHdr) + trans->prepad + trans->length + 
		  trans->postpad);
	
        beginPtrTX = (ringPtrTX + 
                      ((beginPtrTX - ringPtrTX + length) % (ringSizeTX))); 

        tassert(beginPtrTX < ringPtrTX + ringSizeTX, 
                err_printf("ringCurrentPtrTX out of bounds\n"));

	packetCountTX--;
	freeSpaceTX += length;
    }

    cprintf("PacketRingClient::reclaimSpaceTX - Finish(free=%ld, count=%ld)\n",
            freeSpaceTX, packetCountTX);
    return;
}


/*
 * It is responsible for waking up blocked threads waiting for data.  
 */
/* virtual */ SysStatus
PacketRingClient::_notifyDataReceive()
{
    recvLock.acquire();

    packetsReceiveNotify++;
    cprintf("PacketRing::notifyReceiveOnThread: seen=%ld\n", 
            packetsReceiveNotify);
    
    threadsBlockedRX = 0;
    DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *)&threadQueueRX);

    recvLock.release();

    if (recvCallback) {
        recvCallback(recvCallbackArg);
    }

    return 0;
}


/*
 * It is responsible for reclaiming transmit ring space
 */
/* virtual */ SysStatus
PacketRingClient::_notifyDataTransmit()
{
    AutoLock<BLock> al(&transLock); // locks now, unlocks on return

    //    cprintf("PacketRing::notifyTransmitOnThread\n");

    locked_reclaimSpaceTX();
    
    threadsBlockedTX = 0;
    DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *)&threadQueueTX);
    
    return 0;
}












