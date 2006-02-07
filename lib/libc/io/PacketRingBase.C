/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingBase.C,v 1.1 2000/08/25 19:59:43 ggoodson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines base packet ring structure
 *****************************************************************************/

#include <io/PacketRingBase.H>
#include <sync/atomic.h>

/* virtual */ SysStatus
PacketRingBase::RingHdrValid(uval ringPtr)
{
    SyncBeforeRelease();
    
    return ( ((PacketRingHdr *)(ringPtr))->flags & RING_HDR_FLAGS_VALID );
}

/* virtual */ void
PacketRingBase::RingHdrInvalidate(uval ringPtr)
{
    SyncBeforeRelease();
    ((PacketRingHdr *)(ringPtr))->flags &= ~RING_HDR_FLAGS_VALID;
    SyncAfterAcquire();
}

/* virtual */ void
PacketRingBase::RingHdrValidate(uval ringPtr)
{
    SyncBeforeRelease();
    ((PacketRingHdr *)(ringPtr))->flags |= RING_HDR_FLAGS_VALID;
    SyncAfterAcquire();
}
