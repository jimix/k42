/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COList.C,v 1.1 2005/01/26 03:21:49 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/****************************************************************************
 * Module Description: 
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "COList.H"
#include "COListServer.H"
#include <stub/StubRegionDefault.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubSchedulerService.H>
#include <mem/Access.H>

/* static */ SysStatus
COList::Create(COListRef &ref, ProcessID pid) {
    COList *rep = new COList();
    ref = (COListRef)CObjRootSingleRep::Create(rep);
    SysStatus rc =  rep->init(pid);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    return rc;
}

SysStatus 
COList::init(ProcessID pid)
{
    srvPID = pid;
    // connstruct an ObjectHandle to SchedulerService of srvPID
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    schedServ.initOHWithPID(
        srvPID,
        XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));

    // Ask SchedulerService to create COListServer and get back OH
    ObjectHandle COLstSrvOH;
    SysStatus rc = schedServ._initCODescs(COLstSrvOH);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    
    // init server stub with OH of server object
    coLstSrv.setOH(COLstSrvOH);

    // Ask server for FR of CODescArray
    ObjectHandle froh;
    rc = coLstSrv.getCODescsFR(froh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    // map CODescArray
    uval addr;
    //FIXME: change this so that this side of the mapping is read only
    rc = StubRegionDefault::_CreateFixedLenExt(
	addr, COListServer::SizeOfRegion, 0, froh, 0,
	AccessMode::writeUserWriteSup, 0, RegionType::K42Region);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    coDescArray = (CODesc *)addr;

    err_printf("COList::init: this=%p ref=%p, coDescArray=%p srvPid=%ld\n",
               this, getRef(), coDescArray, srvPID);
    return rc;
}

