/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COListServer.C,v 1.3 2005/03/02 05:27:55 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "COListServer.H"
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <cobj/CObjRootSingleRep.H>
#include <mem/Access.H>
#include <stub/StubIPSock.H>
#include <io/FileLinuxSocket.H>
#include <io/Socket.H>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


SysStatus
COListServer::init(ProcessID clntPid) 
{
    clientPid = clntPid;

    // create FR for shared memory to be used between us and the call pid
    ObjectHandle froh;
    SysStatus rc = StubFRComputation::_Create(froh);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    fr.setOH(froh);

    // create region to be used to access shared memory
    uval addr;
    rc = StubRegionDefault::_CreateFixedLenExt(
        addr, SizeOfRegion, 0, froh, 0, AccessMode::writeUserWriteSup, 0,
        RegionType::K42Region);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    coDescArray = (CODesc *)addr;
    err_printf("COListServer::init: this=%p ref=%p coDescArray=%p"
               " clientPid=%ld\n",
               this, getRef(), coDescArray, clientPid);
    return rc;
}

/* static */ SysStatus 
COListServer::Create(ObjectHandle &oh,
                     ProcessID pid)
{
    
    COListServer *rep = new COListServer();
    COListServerRef ref = (COListServerRef)CObjRootSingleRep::Create(rep);
    SysStatus rc = rep->init(pid);
    if (_SUCCESS(rc)) {
        rc = DREF(ref)->giveAccessByServer(oh, pid);
        tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    }
    return rc;
}

/* virtual */ SysStatus
COListServer::getCODescsFR(__out ObjectHandle &oh, __CALLER_PID pid)
{
    if (pid != clientPid) {
        tassertMsg(0, "pid = %ld != clientPid = %ld\n", pid, clientPid);
        return -1;
    }
    SysStatus rc =  fr._giveAccess(oh, clientPid);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    return rc;
}

/* virtual */ SysStatus 
COListServer::updateCODescs(__out uval & numCODesc)
{
//    err_printf("updateCODescs called\n");
    /* apw */ uval bogus = SizeOfRegion / sizeof(CODesc);
    SysStatus rc = DREFGOBJ(TheCOSMgrRef)->getCOList(coDescArray, bogus);
    tassertMsg(_SUCCESS(rc), "getCOList returned %ld\n", rc);
    tassertMsg(((uval)rc * sizeof(CODesc)) <= SizeOfRegion,
                "%ld bytes is not <= %d buffer\n",  
               ((uval)rc * sizeof(CODesc)),
               SizeOfRegion);
    numCODesc = rc;
    return 0;
}

/* virutal */ SysStatus
COListServer::hotSwapInstance(__in uval fac, __in uval target)
{
    // FIXME: get rid ot type cheats
    FactoryRef facRef = (FactoryRef)fac;
    CObjRoot *oldRoot, *newRoot;
    CORef targetRef;

    oldRoot = (CObjRoot *)target;
    targetRef = (CORef)oldRoot->getRef();

    /************ SUPER FIXME:  No real checking going on here ************/
    SysStatus rc = DREF(facRef)->createReplacement(targetRef, newRoot);
    passertMsg(_SUCCESS(rc), "oops createReplacment failed rc=%ld\n",
               rc);
    oldRoot->deRegisterFromFactory();
    rc = COSMgr::switchCObj(targetRef,newRoot);
    passertMsg(_SUCCESS(rc), "oops switchCObj failed rc=%ld\n",
               rc);
    return rc;
}

/* virutal */ SysStatus
COListServer::takeOverFromFac(__in uval newFac, __in uval oldFac)
{
    // FIXME: get rid ot type cheats
    FactoryRef newRef = (FactoryRef)newFac;
    FactoryRef oldRef = (FactoryRef)oldFac;
    
    /************ SUPER FIXME:  No real checking going on here ************/
    SysStatus rc = DREF(newRef)->takeOverFromFac(oldRef);
    passertMsg(_SUCCESS(rc), "oops takeOverFromFac failed rc=%ld\n",
               rc);
    return rc;
}

/* virutal */ SysStatus 
COListServer::printInstances(__in uval fac)
{
    FactoryRef facRef = (FactoryRef)fac;
    return DREF(facRef)->printInstances();
}

/* virtual */ SysStatus
COListServer::readMem(__in uval addr, __inout uval & len, char *buf)
{
    // FIXME:  add test for validity of address range
    if (len > 2048) { len=0; return -1; }
    memcpy(buf, (char *)addr, len);
    return 0;
}
