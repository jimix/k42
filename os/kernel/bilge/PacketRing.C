/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRing.C,v 1.4 2001/05/25 16:31:49 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for network packet rings
 ***************************************************************************/

#include "kernIncs.H"
#include <bilge/PacketRing.H>
#include <meta/MetaPacketRing.H>
#include <stub/StubPacketRing.H>
#include <sys/BaseProcess.H>
#include <proc/Process.H>
#include <sys/ProcessSet.H>
#include <mem/FCMFixed.H>
#include <mem/RegionDefault.H>

extern SysStatus xio_transmit_packet(char *part1, int size1, 
                                     char *part2, int size2, 
                                     PacketRingRef pRef);

void
PacketRing::ClassInit(VPNum vp)
{
    if (vp != 0) return;
    MetaPacketRingServer::init();
}

/* static */ SysStatus
PacketRing::InternalCreate(PacketRingRef &pRef, uval txSize, uval rxSize,
			   uval &vaddrTX, uval &vaddrRX, ProcessID caller)
{
    PacketRing *pr = new PacketRing;
    SysStatus rc;

    if (pr == NULL) {
	return _SERROR(1656, 0, EINVAL);
    }
    
    rc = pr->init(caller, vaddrTX, vaddrRX, txSize, rxSize);
    if (_FAILURE(rc)) {
	return rc;
    }
    
    pRef = (PacketRingRef)CObjRootSingleRep::Create(pr);
    pr->packetRingRef = pRef;

    cprintf("PacketRing::InternalCreate: succeeded...\n");

    return 0;
}


/* virtual */ SysStatus
PacketRing::init(ProcessID caller, uval &vaddrTX, uval &vaddrRX, uval txSize, 
                 uval rxSize)
{
    SysStatus rc;
    uval size;

    refCount = 0;
    beingDestroyed = 0;
    pageArrayNumTX = 0;
    pageArrayNumRX = 0;
    packetCountRX = 0;

    ringCurrentIdxRX = 0;
    ringCurrentOffsetRX = 0;

    ringCurrentIdxTX = pageArrayNumRX;
    ringCurrentOffsetTX = 0;

    beginIdxRX = 0;
    beginOffsetRX = 0;

    beginIdxTX = pageArrayNumRX;
    beginOffsetTX = 0;

    packetsReceived = 0;
    packetsTransmitted = 0;
    packetsDropped = 0;
    packetsRecvCoalesced = 0;

    fcmOffset = 0;

    // Get references for the process
    DREFGOBJ(TheProcessSetRef)->getRefFromPID(caller, 
					      (BaseProcessRef &)processRef);

    // Create new FCM
    rc = FCMFixed<AllocGlobal>::Create(fcmRef);
    if (_FAILURE(rc)) {
	err_printf("create fcm failed: %#lx\n", rc);
	return rc;
    }

    size = PAGE_ROUND_UP(rxSize) + PAGE_ROUND_UP(txSize) + PAGE_SIZE;

    // Create shared region
    rc = RegionDefault::CreateFixedLen(regionRef, processRef, baseVaddr, size,
                                       0, fcmRef, 0, 
				       AccessMode::writeUserWriteSup);
    if (_FAILURE(rc)) {
        err_printf("create region failed: %#lx\n", rc);
        return rc;
    }
    
    // Allocate packet rings
    rc = allocRings(PAGE_ROUND_UP(txSize), PAGE_ROUND_UP(rxSize)); 
    if (_FAILURE(rc)) {
        err_printf("allocRings failed\n");
	return rc;
    }

    // Allocate ctrl region
    rc = allocCtrl();
    if (_FAILURE(rc)) {
        err_printf("allocCtrl failed\n");
        return rc;
    }

    vaddrRX = baseVaddr;
    vaddrTX = baseVaddr + PAGE_ROUND_UP(rxSize);
    
    return 0;
}


/* virtual */ SysStatus
PacketRing::destroy()
{
    uval i, fcmCurrentOffset;

    // remove all ObjRefs to this object
    SysStatus rc=closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;

    fcmCurrentOffset = 0;

    // Go through and unmap the pages of recv/trans rings
    for (i = 0; i < (pageArrayNumTX + pageArrayNumRX); i++) {
	DREF(fcmRef)->disEstablishPage(fcmCurrentOffset, PAGE_SIZE);
	DREF(fcmRef)->removeReference();
	fcmCurrentOffset += PAGE_SIZE;
    }

    // Unmap pages of ctrl structure
    DREF(fcmRef)->disEstablishPage(fcmCurrentOffset, PAGE_SIZE);
    DREF(fcmRef)->removeReference();
    
    cprintf("Packets Transmitted:    %lu\n", packetsTransmitted);
    cprintf("Packets Received:       %lu\n", packetsReceived);
    cprintf("Packets Dropped:        %lu\n", packetsDropped);
    cprintf("Packets Recv Coalesced: %lu\n", packetsRecvCoalesced);

    // schedule the object for deletion
    destroyUnchecked();

    return 0;
}


// Called when last xobject released see Obj.H
/* virtual */ SysStatus 
PacketRing::exportedXObjectListEmpty()
{
    if (refCount == 0) {
        return destroy();
    } else {
        beingDestroyed = 1;
    }

    return 0;
}


SysStatus
PacketRing::allocCtrl()
{
    SysStatus rc;

    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(ctrlPage, PAGE_SIZE);
    tassert(_SUCCESS(rc), err_printf("allocPages failed - ctrl page\n"));   

    cprintf("Created page at: %#lx\n", ctrlPage);

    // FIXME: need better handling of these errors
    rc = DREF(fcmRef)->addReference();
    tassert(_SUCCESS(rc), err_printf("addReference failed - ctrl page\n"));
    
    rc = DREF(fcmRef)->establishPage(fcmOffset, ctrlPage, PAGE_SIZE);
    tassert(_SUCCESS(rc), err_printf("establishPage failed - ctrl page\n"));
    
    fcmOffset += PAGE_SIZE;

    ctrlRegister = (volatile uval32 *)ctrlPage;
    AtomicAnd32Synced(ctrlRegister, 0);

    return 0;
}


SysStatus
PacketRing::allocRings(uval txSize, uval rxSize)
{
    uval i, rc;

    // Check that size is valid 
    if ((txSize <= 0) || (txSize > (MAX_PAGES << LOG_PAGE_SIZE))) {
	err_printf("invalid tx size\n");
	return _SERROR(1657, 0, EINVAL);
    }

    if ((rxSize <= 0) || (rxSize > (MAX_PAGES << LOG_PAGE_SIZE))) {
	err_printf("invalid rx size\n");
	return _SERROR(1658, 0, EINVAL);
    }

    pageArrayNumTX = txSize >> LOG_PAGE_SIZE;
    pageArrayNumRX = rxSize >> LOG_PAGE_SIZE;

    // Go through and alloc new pages, incr FCM ref count, and map them
    for (i = 0; i < (pageArrayNumRX + pageArrayNumTX); i++) {
        // FIXME: need better handling of these errors
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(pageArray[i], 
							      PAGE_SIZE);
	tassert(_SUCCESS(rc), err_printf("allocPages failed, iter %ld", i));

	rc = DREF(fcmRef)->addReference();
	tassert(_SUCCESS(rc), err_printf("addReference failed, iter %ld", i));

	rc = DREF(fcmRef)->establishPage(fcmOffset, pageArray[i], PAGE_SIZE);
	tassert(_SUCCESS(rc), err_printf("establishPage failed, iter %ld", i));

	fcmOffset += PAGE_SIZE;
    }

    ringCurrentIdxTX = pageArrayNumRX;
    beginIdxTX = pageArrayNumRX;

    // Zero out beginning headers of packet rings
    memset((void *)pageArray[ringCurrentIdxRX], 0, sizeof(PacketRingHdr));
    memset((void *)pageArray[ringCurrentIdxTX], 0, sizeof(PacketRingHdr));

    freeSpaceRX = rxSize - sizeof(PacketRingHdr);

    return 0;
}

/* private */ void
PacketRing::reclaimSpaceRX()
{
    PacketRingHdr *recvHdr;
    uval length;

    cprintf("PacketRing::reclaimSpace - (freeSpace=%ld, pckt count=%ld)\n", 
            freeSpaceRX, packetCountRX);

    recvHdr = (PacketRingHdr *)(pageArray[beginIdxRX] + beginOffsetRX);
  
    while((!RingHdrValid((uval)recvHdr)) && packetCountRX > 0) {

	length = (sizeof(PacketRingHdr) + recvHdr->prepad + recvHdr->length + 
		  recvHdr->postpad);
	beginOffsetRX += length;

	if (beginOffsetRX >= PAGE_SIZE) {
	    beginOffsetRX = beginOffsetRX % PAGE_SIZE;
	    beginIdxRX = (beginIdxRX + 1) % pageArrayNumRX;
	}
	packetCountRX--;
	freeSpaceRX += length;
        
        tassert(beginOffsetRX < PAGE_SIZE, 
                err_printf("beginOffsetRX out of bounds\n"));
        tassert(beginIdxRX >= 0 && 
                beginIdxRX < pageArrayNumRX,
                err_printf("beginIdxRX out of bounds\n"));

	recvHdr = (PacketRingHdr *)(pageArray[beginIdxRX] + beginOffsetRX);
    }

    cprintf("PacketRing::reclaimSpace - End (freeSpace=%ld)\n", freeSpaceRX);

    return;
}

/* virtual */ void
PacketRing::receivedPacket(char *packet, uval length)
{
    PacketRingHdr *recvHdr, *nextHdr;
    
    uval nextHdrIdx, nextHdrOffset;
    uval len, postpad=0, prepad=0, usedSpace, tmp;
    uval32 oldCtrlReg;

    uval tmpOffsetRX, tmpIdxRX;

#if 0 // To test dropped packets
    static uval packetsIn = 0;
    packetsIn++;
    if (packetsIn % 5 == 0) {
        cprintf("PacketRing::receivedPacket - Dropping packet\n");
        packetsDropped++;
        return;
    }
#endif

    cprintf("PacketRing::receivedPacket (length=%ld, free=%ld)\n", 
            length, freeSpaceRX);

    cprintf("PacketRing::receivedPacket - placed at (idx=%ld, off=%ld)\n",
            ringCurrentIdxRX, ringCurrentOffsetRX);

    recvHdr = (PacketRingHdr *)(pageArray[ringCurrentIdxRX] + 
                                ringCurrentOffsetRX);

    tmpIdxRX = ringCurrentIdxRX;
    tmpOffsetRX = ringCurrentOffsetRX + sizeof(PacketRingHdr);

    if (tmpOffsetRX >= PAGE_SIZE) {
	tmpOffsetRX = 0;
	tmpIdxRX = (tmpIdxRX + 1) % pageArrayNumRX;
    }

    /* First figure out where next packet is going to go 
     * Make sure ring hdrs are aligned and do not overlap pages 
     * This only works if hdr size is a multiple of the alignment size
     */
    nextHdrIdx = tmpIdxRX;
    tmp = ALIGN_UP(length + tmpOffsetRX, 8);
    nextHdrOffset = tmp % PAGE_SIZE;
    postpad = tmp - (length + tmpOffsetRX);

    if ((tmp + sizeof(PacketRingHdr)) > PAGE_SIZE) {
        nextHdrIdx++;

	if (tmp <= PAGE_SIZE) {
	    nextHdrOffset = 0;
	    postpad += PAGE_SIZE - tmp;
	}

#if 1
        /* Due to not handling variables crossing page boundaries we must
         * ensure that packets don't cross page boundaries
         */
        if (nextHdrOffset != 0) {
	    nextHdrOffset = ALIGN_UP(length, 8);
	    nextHdrIdx = ((nextHdrIdx + 1) % pageArrayNumRX) - 1;
	    prepad = PAGE_SIZE - tmpOffsetRX;
	    postpad = nextHdrOffset - length;
        }

#else   // FIXME: this code should be used if it weren't for the bug accessing
        // variables across page boundaries

	/* Don't want data to wrap around ring edge */
	if (nextHdrIdx >= pageArrayNumRX && nextHdrOffset != 0) {
	    nextHdrOffset = ALIGN_UP(length, 8);
	    nextHdrIdx = 0;
	    prepad = PAGE_SIZE - tmpOffsetRX;
	    postpad = nextHdrOffset - length;
	}
#endif

        nextHdrIdx = nextHdrIdx % pageArrayNumRX;
    }

    // Calculate space taken by this packet
    usedSpace = prepad + length + postpad + sizeof(PacketRingHdr);

    cprintf("PacketRing::receivedPacket (prepad=%ld, postpad=%ld, used=%ld)\n",
            prepad, postpad, usedSpace);
    
    if (usedSpace > freeSpaceRX) {
	reclaimSpaceRX();
	if (usedSpace > freeSpaceRX) {
            packetsDropped++;
            cprintf("PacketRing::receivedPacket - dropping packet (%ld)\n",
                    packetsDropped);
	    return;
	}
    }

    // Restore state
    ringCurrentOffsetRX = tmpOffsetRX;
    ringCurrentIdxRX = tmpIdxRX;

    freeSpaceRX -= usedSpace;
    packetCountRX++;

    // Increment due to change in prepadding (if necessary)
    ringCurrentOffsetRX += prepad;
    if (ringCurrentOffsetRX >= PAGE_SIZE) {
	ringCurrentIdxRX = (ringCurrentIdxRX + 1) % pageArrayNumRX;
	ringCurrentOffsetRX = ringCurrentOffsetRX % PAGE_SIZE;
    }
          
    nextHdr = (PacketRingHdr *)(pageArray[nextHdrIdx] + nextHdrOffset);
    memset(nextHdr, 0, sizeof(PacketRingHdr));

    tmp = length;
    // Copy packet to correct spot
    while(length > 0) {
	len = MIN(PAGE_SIZE - ringCurrentOffsetRX, length);

        cprintf("PacketRing::receivedPacket - idx: %lu, off: %lu, len: %lu\n",
                ringCurrentIdxRX, ringCurrentOffsetRX, len);

	memcpy((char *)(pageArray[ringCurrentIdxRX] + ringCurrentOffsetRX),
	       packet, len);
	
	ringCurrentOffsetRX += len;
	if (ringCurrentOffsetRX == PAGE_SIZE) {
	    ringCurrentOffsetRX = 0;
	    ringCurrentIdxRX = (ringCurrentIdxRX + 1) % pageArrayNumRX;
	}

	packet += len;
	length -= len;	
        tassert(length == 0, 
                err_printf("PacketRing::receivedPacket - overlap\n"));
    }

    // Set ring current offset
    ringCurrentIdxRX = nextHdrIdx;
    ringCurrentOffsetRX = nextHdrOffset;

    tassert(ringCurrentOffsetRX < PAGE_SIZE, 
            err_printf("ringCurrentOffsetRX out of bounds\n"));
    tassert(ringCurrentIdxRX >= 0 && 
            ringCurrentIdxRX < pageArrayNumRX,
            err_printf("ringCurrentIdxRX out of bounds\n"));

    // Fill in hdr
    recvHdr->length = tmp;
    recvHdr->prepad = prepad;
    recvHdr->postpad = postpad;

    RingHdrValidate((uval)recvHdr); // Sets bit using sync commands

    packetsReceived++;

    oldCtrlReg = FetchAndOr32Synced(ctrlRegister, CHECKING_READ);

    cprintf("PacketRing::receivedPacket - (count=%ld, total=%ld, ctrl=%#x)\n", 
            packetCountRX, packetsReceived, oldCtrlReg);

    // Only send up call if recv intrs are not masked and there was no
    // previous data
    if ((CHECKING_READ &~ oldCtrlReg) && (RECV_INTR_MASK &~ oldCtrlReg)) {
        SysStatus rc;
        rc = stub._notifyDataReceive();
        cprintf("PacketRing::receivedPacket - sending async upcall\n");
        if (_FAILURE(rc)) {
            cprintf("PacketRing::receivedPacket - upcall failed: %#lx\n", rc);
        }
    } else {
        packetsRecvCoalesced++;
    }
    
    return;
}


/* virtual */ SysStatus
PacketRing::bindFilter()
{
    refCount++; // FIXME: Atomic inc??
    cprintf("PacketRing::bindFilter - %ld filters bound\n", refCount);
    return 0;
}


/* virtual */ SysStatus
PacketRing::unbindFilter()
{
    refCount--; // FIXME: Atomic dec??
    tassert(refCount >= 0, err_printf("PacketRing::unbindFilter - cnt=%ld\n",
                                      refCount));
    if (beingDestroyed && refCount == 0) {
        destroy();
    }

    return 0;
}

/* virtual */ SysStatus
PacketRing::_registerCallbackObj(__in ObjectHandle callbackObj,
				 __XHANDLE xhandle)
{
    stub.setOH(callbackObj);
    
    return 0;
}

/* virtual */ void
PacketRing::transmittedPacket()
{
    PacketRingHdr *transHdr;

    cprintf("PacketRing::transmittedPacket - callback\n");

    tassert(beginIdxTX >= pageArrayNumRX && 
            beginIdxTX < pageArrayNumRX + pageArrayNumTX,
            err_printf("ringCurrentIdxTX out of bounds\n"));

    transHdr = (PacketRingHdr *)(pageArray[beginIdxTX] + beginOffsetTX);

    RingHdrInvalidate((uval)transHdr); // Invalidate header using sync commands
    
    // Set new begin offset and index
    beginOffsetTX += (sizeof(PacketRingHdr) + transHdr->prepad + 
                      transHdr->postpad + transHdr->length);

    if (beginOffsetTX >= PAGE_SIZE) {
	beginOffsetTX = beginOffsetTX % PAGE_SIZE;
	beginIdxTX = (pageArrayNumRX + ((beginIdxTX - pageArrayNumRX + 1) % 
					pageArrayNumTX));
    }

    if ((*ctrlRegister & TRANS_INTR_MASK) == 0) {
        stub._notifyDataTransmit();
    }

    tassert(beginOffsetTX < PAGE_SIZE, 
            err_printf("beginOffsetTX out of bounds\n"));
    tassert(beginIdxTX >= pageArrayNumRX && pageArrayNumRX + pageArrayNumTX, 
            err_printf("beginIdxTX out of bounds\n"));
}


/* virtual */ SysStatus
PacketRing::_transmit()
{
    PacketRingHdr *transHdr;
    SysStatus rc;
    uval dataOffset, dataIdx, len, dataOffset2=0, dataIdx2=0, len2=0;

    transHdr = (PacketRingHdr *)(pageArray[ringCurrentIdxTX] + 
                                 ringCurrentOffsetTX);  

    cprintf("PacketRing::_transmit (Hdr: idx=%ld, off=%ld)\n",
            ringCurrentIdxTX - pageArrayNumRX, ringCurrentOffsetTX);

    while(RingHdrValid((uval)transHdr)) {

	dataIdx = ringCurrentIdxTX;
	dataOffset = ringCurrentOffsetTX + (sizeof(PacketRingHdr) + 
                                            transHdr->prepad);
	len = transHdr->length;

	// Set start of data
	if (dataOffset >= PAGE_SIZE) {
	    dataOffset = dataOffset % PAGE_SIZE;
	    dataIdx = (pageArrayNumRX + 
		       ((ringCurrentIdxTX - pageArrayNumRX + 1) % 
			pageArrayNumTX));
	}

	// Check if data overlaps onto two pages
	if (dataOffset + len > PAGE_SIZE) {
	    dataIdx2 = (pageArrayNumRX + 
			((dataIdx - pageArrayNumRX + 1) % pageArrayNumTX));
	    dataOffset2 = 0;
	    len = PAGE_SIZE - dataOffset;
	    len2 = transHdr->length - len;
	}

        cprintf("PacketRing::_transmit: (idx1=%ld, off1=%ld, size1=%ld)\n",
                dataIdx - pageArrayNumRX, dataOffset, len);

        cprintf("PacketRing::_transmit: (idx2=%ld, off2=%ld, size2=%ld)\n",
                dataIdx2 - pageArrayNumRX, dataOffset2, len2);

	rc = xio_transmit_packet((char *)(pageArray[dataIdx] + dataOffset),
				 len, 
				 (char *)(pageArray[dataIdx2] + dataOffset2),
				 len2, packetRingRef);

	tassert(rc >= 0, err_printf("xio_transmit_packet: failed\n"));

        packetsTransmitted++;

        cprintf("PacketRing::_transmit - Transmitted Packet (len=%ld, "
                "total=%ld)\n", len + len2, packetsTransmitted);

	// Set new begin offset and index
	ringCurrentOffsetTX += (sizeof(PacketRingHdr) + transHdr->prepad + 
				transHdr->postpad + transHdr->length);
	if (ringCurrentOffsetTX >= PAGE_SIZE) {
	    ringCurrentOffsetTX = ringCurrentOffsetTX % PAGE_SIZE;
	    ringCurrentIdxTX = (pageArrayNumRX + 
				((ringCurrentIdxTX - pageArrayNumRX + 1) % 
				 pageArrayNumTX));
	}

	transHdr = (PacketRingHdr *)(pageArray[ringCurrentIdxTX] + 
                                     ringCurrentOffsetTX);
    }

    tassert(ringCurrentOffsetTX < PAGE_SIZE, 
            err_printf("ringCurrentOffsetTX out of bounds\n"));
    tassert(ringCurrentIdxTX >= pageArrayNumRX && 
            ringCurrentIdxTX < pageArrayNumRX + pageArrayNumTX,
            err_printf("ringCurrentIdxTX out of bounds\n"));

    return 0;
}


/* virtual */ SysStatus
PacketRing::_destroy()
{
    return destroy();
}


