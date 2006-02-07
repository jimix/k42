#ifndef __RES_MGR_WRAPPER_H_
#define __RES_MGR_WRAPPER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ResMgrWrapper.H,v 1.20 2004/01/05 18:26:40 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for calling the resource manager
 * **************************************************************************/

#include <stub/StubResMgr.H>
#include <meta/MetaResMgr.H>

class ResMgrWrapper : public Obj {
    StubResMgr stub;
protected:
    DEFINE_GLOBAL_NEW(ResMgrWrapper);
    ResMgrWrapper() : stub(StubObj::UNINITIALIZED) { }
public:

    static SysStatus Create(ObjectHandle serverOH);
    static SysStatus Create();
    static SysStatus Create(ResMgrWrapperRef& ref);
    static SysStatus CreateAndRegisterFirstDispatcher();

    virtual SysStatus postFork();

    virtual SysStatus assignDomain(uval uid);

    virtual SysStatus createFirstDispatcher(ObjectHandle childOH,
					    EntryPointDesc entry,
					    uval dispatcherAddr,
					    uval initMsgLength,
					    char *initMsg);


    virtual SysStatus createDispatcher(DispatcherID dspid,
				       EntryPointDesc entry,
				       uval dispatcherAddr,
				       uval initMsgLength,
				       char *initMsg);

    virtual SysStatus execNotify();

    // FIXME:  this interface to the res mgr is for testing and should go away.
    virtual SysStatus migrateVP(VPNum vpNum, VPNum suggestedPP);

    // FIXME:  this interface should be restricted to privileged clients.
    virtual SysStatus mapKernelSchedulerStats(uval &statsRegionAddr,
					      uval &statsRegionSize,
					      uval &statsSize);

    virtual SysStatus setStatsFlag(uval val);
    virtual SysStatus toggleStatsFlag();
};
#endif /* #ifndef __RES_MGR_WRAPPER_H_ */
