/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiterCallCounter.H,v 1.1 2004/01/24 20:58:15 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __COBJREPARBITERCALLCOUNTER
#define __COBJREPARBITERCALLCOUNTER

#include <cobj/CObjRootArbiter.H>

class CORepArbiterCallCounter;

typedef CORepArbiterCallCounter** cocccRef;

class CORepArbiterCallCounter : public CORepArbiter{
protected:
    virtual SysStatus handleCall(CallDescriptor* cd, uval fnum);
    sval callCount[256];

    class CORootArbiterCC : public CORootArbiter{
    friend class CORepArbiterCallCounter;
    protected:
        DEFINE_GLOBAL_NEW(CORootArbiterCC);
	CORootArbiterCC() {}
        virtual CObjRep* createRep(VPNum vp);
    public:
        static cocccRef Create();
    };

    friend class CORootArbiterCC;
    CORepArbiterCallCounter() {
        for(int i = 0; i < 256; i++){
            callCount[i] = 0;
        }
    }
    DEFINE_LOCALSTRICT_NEW(CORepArbiterCallCounter);
public:
    virtual SysStatus getCallCount(sval count[256]);
    static cocccRef Create();
};

#endif
