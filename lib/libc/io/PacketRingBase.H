/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingBase.H,v 1.2 2001/10/05 21:47:42 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines base packet ring structure
 *****************************************************************************/

#ifndef __PACKET_RING_BASE_H_
#define __PACKET_RING_BASE_H_

#include <cobj/Obj.H>

class PacketRingBase : public Obj {

public:
    enum {RING_HDR_FLAGS_VALID=0x1};
    enum {RING_HDR_INVALID=0, RING_HDR_VALID=1, ALIGN_DATA=8};
    enum {RECV_INTR_MASK=0x1, TRANS_INTR_MASK=0x2, CHECKING_READ=0x4,
          DATA_AVAIL_WRITE=0x8};

    struct PacketRingHdr {
        uval16 flags;
        uval16 length;
        uval16 prepad;
        uval16 postpad;
    };

    virtual SysStatus RingHdrValid(uval ringPtr);

    virtual void RingHdrInvalidate(uval ringPtr);

    virtual void RingHdrValidate(uval ringPtr);
};

#endif /* #ifndef __PACKET_RING_BASE_H_ */
