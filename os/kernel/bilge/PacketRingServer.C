/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketRingServer.C,v 1.2 2000/12/10 10:57:41 okrieg Exp $
 *****************************************************************************/

#include <bilge/PacketRing.H>
#include <stub/StubPacketRingServer.H>

void
PacketRingServer::ClassInit(VPNum vp)
{
    PacketRing::ClassInit(vp);
}

/* static */ SysStatus
PacketRingServer::_Create(__in uval txSize, __in uval rxSize, 
			  __out uval &vaddrTX, __out uval &vaddrRX,
			  __out ObjectHandle &prOH, __CALLER_PID caller)
{
    SysStatus rc;
    PacketRingRef pRef;

    rc = PacketRing::InternalCreate(pRef, txSize, rxSize, vaddrTX, vaddrRX,
				    caller);
    if (_FAILURE(rc)) return rc;
    
    // FIXME: figure out a way to combine xobject holder list with
    // the stubobject holder indicated above
    rc = DREF(pRef)->giveAccessByServer(prOH, caller);
    if (_FAILURE(rc)) return rc;
    
    return 0;
}


/* virtual */ SysStatus
PacketRingServer::getType(TypeID &id)
{
    id = StubPacketRingServer::typeID();
    return 0;
}


