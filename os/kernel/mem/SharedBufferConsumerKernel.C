/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferConsumerKernel.C,v 1.2 2005/05/24 02:59:30 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include "kernIncs.H"
#include "SharedBufferConsumerKernel.H"
#include <mem/Access.H>
#include <cobj/XHandleTrans.H>
#include "RegionDefault.H"

/* virtual */ SysStatus 
SharedBufferConsumerKernel::initShMem(ObjectHandle sfrOH, ProcessID pidProducer,
				      uval sz, uval &shAddr)
{
    SysStatus rc;
    ObjRef objRef;
    FRRef frRef;
    TypeID type;
    
    rc = XHandleTrans::XHToInternal(sfrOH.xhandle(), pidProducer,
				    MetaObj::attach, objRef, type);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);
    
    // verify that type is of FR
    if (!MetaFR::isBaseOf(type)) {
	tassertMsg(0, "object handle <%lx,%lx> not of correct type\n",
		   sfrOH.commID(), sfrOH.xhandle());
	return _SERROR(29020, 0, EINVAL);
    }
    
    frRef = (FRRef)objRef;	
    // create region to be used as shared
    RegionRef rref;
    rc = RegionDefaultKernel::CreateFixedLen(
	rref, GOBJK(TheProcessRef), shAddr, sz, 0/*aligmentreq*/,
	frRef, 1/*writable*/, 0/*fileOffset*/, AccessMode::writeUserWriteSup);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    return rc;
}
