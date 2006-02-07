/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExec.C,v 1.8 2004/03/03 16:02:22 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel version of ProgExec::MapModule
 * **************************************************************************/

#include <kernIncs.H>
#include <usr/ProgExec.H>
#include <proc/Process.H>
#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessServer.H>

/*static*/ void
ProgExec::ConsoleGiveAccess(ObjectHandle& ptyOH, ProcessID targetPID)
{
    /*
     * Processes started by the kernel get access to the kernel Console
     * object.
     */
    SysStatus rc;
    rc = ((Obj *)DREFGOBJK(TheConsoleRef))->
				giveAccessByClient(ptyOH, targetPID);
    tassertMsg(_SUCCESS(rc), "pty access failed\n");
}

/*static*/ void
ProgExec::ProcessLinuxGiveAccess(ObjectHandle& procLinuxOH, ObjectHandle procOH)
{
    /*
     * Processes started by the kernel are not known (at least initially)
     * to the ProcessLinux server.
     */
    procLinuxOH.init();
}

/*static*/ SysStatus
ProgExec::CreateFirstDispatcher(ObjectHandle childOH,
			   EntryPointDesc entry, uval dispatcherAddr,
			   uval initMsgLength, char *initMsg)
{
    /*
     * In the kernel we can create a dispatcher in the child directly.
     */
    SysStatus rc;
    ObjRef ref;
    TypeID type;
    rc = XHandleTrans::XHToInternal(childOH.xhandle(), _KERNEL_PID, 0,
				    ref, type);
    passertMsg(_SUCCESS(rc), "Couldn't get ref from process xhandle.\n");
    passertMsg(MetaProcessServer::isBaseOf(type), "Wrong type.\n");
    CPUDomainAnnex *cda = exceptionLocal.serverCDA;
    return DREF(ProcessRef(ref))->
	    createDispatcher(cda, SysTypes::DSPID(0,0), entry, dispatcherAddr,
			     initMsgLength, initMsg);
}

SysStatus
ProgExec::CreateVP(VPNum vp)
{
    return 0;
}

SysStatus
ProgExec::LoadAllModules(ProcessID newPID, XferInfo &xferInfo,
			 XHandle procXH, EntryPointDesc &entry)
{
    MemRegion *region = &xferInfo.region[0];
    ExecInfo &eInfo = xferInfo.exec;

    SysStatus rc = 0;
    //
    // The child process needs access to the FR's in order to load
    // them itself, and to ensure they stay open until that occurs.
    //
    rc = Obj::GiveAccessByClient(eInfo.prog.localFR, eInfo.prog.frOH, newPID);
    _IF_FAILURE_RET(rc);

    // we enter app via the k42 library
    entry = eInfo.entry;

    rc = ProgExec::MapModule(&eInfo.prog, procXH, region, XferInfo::TEXT,
			     RegionType::FreeOnExec);


    passertMsg(_SUCCESS(rc),"couldn't map baseServers: %lx\n",rc);
    _IF_FAILURE_RET(rc);

    rc = ProgExec::ConfigStack(&eInfo);
    return rc;
}
