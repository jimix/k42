/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitServer.C,v 1.2 2004/11/05 16:24:00 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An interface for registering and invoking a step of the
 *		       system initialization process.
 * **************************************************************************/

#include "kernIncs.H"
#include "InitServer.H"
#include <meta/MetaInitServer.H>
#include <alloc/alloc.H>
#include <scheduler/Scheduler.H>
#include <stub/StubInitStep.H>
#include <cobj/CObjRootSingleRep.H>

InitServer* InitServer::theServer = NULL;

InitServer::InitServer()
{
    lock.init();
    memset(callStatus, 0, MAX_INIT_STEP * sizeof(void*));
    CObjRootSingleRep::Create(this);
}

/*static*/ void 
InitServer::ClassInit(VPNum vp)
{
    if(vp) return;

    theServer = new InitServer();
    MetaInitServer::init();
}

/*static*/ SysStatus 
InitServer::_DefineAction(__in uval id, __in ObjectHandle oh)
{
    return theServer->defineAction(id, oh);
}

/*virtual*/ SysStatus 
InitServer::defineAction(uval id, ObjectHandle oh)
{
    AutoLock<FairBLock> al(&lock);
    if(callStatus[id]){
	return _SERROR(2742, 0, EALREADY);
    }
    callStatus[id] = new InitCall(oh);
    if(id==0 || (callStatus[id-1] && 
		 callStatus[id-1]->status==InitCall::InitComplete)){
	Scheduler::ScheduleFunction(InitServer::RunInit, id);
    }else if(callStatus[id-1] && callStatus[id-1]->status & 
	     (InitCall::InitInProgress|InitCall::InitRequested)){
	callStatus[id]->status = InitCall::InitRequested;
    }
    return 0;
}

/*static*/ void
InitServer::RunInit(uval id)
{
    theServer->runInit(id);
}


/*virtual*/ SysStatus
InitServer::_complete(SysStatus rc, __XHANDLE xh)
{
    AutoLock<FairBLock> al(&lock);
    if(_FAILURE(rc)){
	err_printf("Initialization failure step %ld: %lx\n", currID, rc);
	callStatus[currID]->status = InitCall::InitFailed;
	return rc;
    }
    callStatus[currID]->status = InitCall::InitComplete;
    ++currID;

    // Try to run the next step
    if(currID < MAX_INIT_STEP && callStatus[currID]){
	StubInitStep stub(StubObj::UNINITIALIZED);
	stub.setOH(callStatus[currID]->oh);
	
	callStatus[currID]->status = InitCall::InitInProgress;
	err_printf("Running initialization next step %ld\n",currID);

	ObjectHandle oh;
	rc = giveAccessByServer(oh, callStatus[currID]->oh.pid());
	tassertMsg(_SUCCESS(rc), "give access failure: %lx\n",rc);
	stub._doAction(oh);
    }
    return 0;
}

/*virtual*/ void
InitServer::runInit(uval id)
{
    SysStatus rc;
    AutoLock<FairBLock> al(&lock);
    StubInitStep stub(StubObj::UNINITIALIZED);
    if(id<MAX_INIT_STEP && callStatus[id]){
	
	stub.setOH(callStatus[id]->oh);
	
	callStatus[id]->status = InitCall::InitInProgress;
	err_printf("Running initialization step %ld\n",id);

	currID = id;
	ObjectHandle oh;
	rc = giveAccessByServer(oh, callStatus[id]->oh.pid());
	tassertMsg(_SUCCESS(rc), "give access failure: %lx\n",rc);

	rc = stub._doAction(oh);
    }
}
