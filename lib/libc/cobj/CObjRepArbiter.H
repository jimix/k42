/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiter.H,v 1.1 2004/01/24 20:58:15 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __COBJ_REP_ARBITER_H_
#define __COBJ_REP_ARBITER_H_

#include <cobj/sys/COSMgr.H>
#include <cobj/CObjRep.H>
#include <cobj/CallDescriptor.H>

class CORepArbiterBase: public BaseObj{
protected:
    // handleCall() should be first 'cause that's the way it's hard-wired in my
    // assembly code.
    virtual SysStatus handleCall(CallDescriptor* cd, uval fnum) = 0;
    CORepArbiterBase() {}
public:
    virtual SysStatus captureTarget(CORef targetRef) = 0;
    virtual SysStatus releaseTarget() = 0;
};

typedef CORepArbiterBase** ArbiterRef;

class CORepArbiter: public CORepArbiterBase{
protected:
    SysStatus makeCall(CallDescriptor* cd, uval fnum);
    SysStatus makeCallAndReturn(CallDescriptor* cd, uval fnum);
    COSTransObject* targetRep; // NOT ArbiterTarget rep; rep of target object
    // dummy rep location for installing (idea courtesy Jonathan)
    LTransEntry targetLTE;
    BLock targetRepLock;
    bool targetRepInstalled;

    // TODO call description must include params, stack
    CORepArbiter() : targetRepInstalled(false) {}
    DEFINE_LOCALSTRICT_NEW(CORepArbiter);

    friend class CORootArbiter;

public:
    virtual SysStatus captureTarget(CORef targetRef);
    virtual SysStatus releaseTarget();
};

extern "C" SysStatus arbiterCallOriginalMethod(CORepArbiter* corc,
		                              CallDescriptor* cd, uval funcPtr);

#endif
