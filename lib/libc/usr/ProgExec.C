/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExec.C,v 1.338 2005/08/11 20:20:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for user-level initialization
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <defines/experimental.H>
#include <sys/KernelInfo.H>
#include <stub/StubObj.H>
#include <cobj/TypeMgr.H>
#include "stub/StubTypeMgrServer.H"
#include "ProgExec.H"
#include <alloc/PageAllocatorUser.H>
#include <sys/BaseProcess.H>
#include <sys/ProcessWrapper.H>
#include "cobj/XHandleTrans.H"
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <trace/traceMem.h>
#include <sys/memoryMap.H>
#include <sync/MPMsgMgrEnabled.H>
#include <sync/MPMsgMgrDisabled.H>
#include <sync/BlockedThreadQueues.H>
#include <sys/ProcessSet.H>
#include <sys/ProcessSetUser.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <scheduler/DispatcherMgr.H>
#include <scheduler/SchedulerService.H>
#include <io/FileLinux.H>
#include <cobj/sys/COSMgrObject.H>
#include <io/IOForkManager.H>
#include <mem/RegionType.H>
#include <sys/ResMgrWrapper.H>
#include <sys/MountPointMgrClient.H>
#include <sys/SystemMiscWrapper.H>
#include <mem/Access.H>
#include <sys/ProcessLinuxClient.H>
#include <io/MemTrans.H>
#include <sys/ShMemBuf.H>
#include <sys/ShMemClnt.H>
#include <usr/GDBIO.H>
#include <stub/StubFRCRW.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubRegionPerProcessor.H>
#include <stub/StubFRComputation.H>
#include <stub/StubRegionRedZone.H>
#include <usr/runProcessCommon.H>
#include <sys/ShMemClnt.H>
#include <sys/ShMemBuf.H>


extern SysStatus LinuxLibInit();
extern SysStatus LinuxPreExec();
static unsigned char ProgExec_ForkLockMem[sizeof(ProgExec::StaticLock)]
    __attribute__ ((aligned (__alignof__ (ProgExec::StaticLock))));
/*static*/ ProgExec::StaticLock * const ProgExec::ProgExec_ForkLock =
    (ProgExec::StaticLock *)&ProgExec_ForkLockMem;


/*static*/ ProgExec::XferInfo *ProgExec::ExecXferInfo = NULL;
/*static*/ uval ProgExec::MultiVP = 0;	// flag used to inhibit fork()
/*static*/ StubTypeMgrServer *ProgExec::BackupTypeMgr;
/*static*/ ProcessID ProgExec::PID;

extern EntryPointDesc ForkChildDesc;
extern EntryPointDesc SecondaryStartDesc;
uval forkStack;

/*====================================================================*/

struct ProgExec::InitInfo {
    uval xferStart;
    uval xferSize;
    uval xferHdr;
    MemoryMgrPrimitive memory;
};

struct ProgExec::ForkInfo {
    SysStatus rc;			// parent rc - no child if _FAILURE
    Thread *forkThread;			// thread that called fork()
    ObjectHandle childOH;
    ObjectHandle childProcessLinuxOH;
    pid_t childLinuxPID;          	// linux pid if a linux process
    ProcessID parentPID;
    ProcessID childPID;
    XHandle childProcessXH;
    ObjectHandle childPtyOH;
    uval schedForkInfo;
};



//
// This loads the "prog" module and it's interpreter, if necessary.
// On return "entry" identifies how to enter this thing.
//
SysStatus
ProgExec::LoadBinary(ExecInfo &eInfo, MemRegion *region, EntryPointDesc *entry)
{
    SysStatus rc = 0;
    *entry = eInfo.entry = eInfo.prog.entryPointDesc;

    TraceOSUserRunULLoader(DREFGOBJ(TheProcessRef)->getPID());

    if (region[XferInfo::TEXT].size==0) {
	// 0 -> arg for XHandle to process object is ourselves
	rc = MapModule(&eInfo.prog, 0, region, XferInfo::TEXT);
	if (_FAILURE(rc)) return rc;
    }

    if (eInfo.prog.interpOffset && region[XferInfo::ITEXT].size==0) {
	BinInfo &interpInfo = eInfo.interp;
	uval interpreterOrigin = LDSO_ADDR;

	/*
	 * load the interpreter
	 * we then need to fiddle so aux tables etc are same as if
	 * main program was statically linked, but entry point is
	 * interpreter - so we do NOT hide the entry variable, but
	 * overwrite the global.
	 *
	 */
	uval interpOffset = interpreterOrigin -
	    interpInfo.seg[interpInfo.textSegIndex].vaddr;

	// relocate text and data of interpreter, and entry point,
	// if needed
	if (interpOffset) {
	    interpInfo.seg[interpInfo.textSegIndex].vaddr += interpOffset;
	    interpInfo.seg[interpInfo.dataSegIndex].vaddr += interpOffset;
	    interpInfo.entryPointDesc.relocate(interpOffset);
	}

	// we enter app via interpreter entry point
	*entry = eInfo.entry = interpInfo.entryPointDesc;

	eInfo.prog.interp = &interpInfo;

	rc = MapModule(&interpInfo, 0, region, XferInfo::ITEXT);

	if (_FAILURE(rc)) return rc;

    } else {
	eInfo.prog.interp = NULL;
    }

    passertMsg(_SUCCESS(rc),"LoadBinary failure: %lx\n",rc);

    if (region[XferInfo::STACK].size==0 && eInfo.stack.memsz) {
	rc = StubRegionDefault::_CreateFixedAddrLenExt(
	    eInfo.stack.vaddr, eInfo.stack.memsz, eInfo.localStackFR, 0,
	    (uval)(AccessMode::writeUserWriteSup), 0,
	    RegionType::ForkCopy);

	_IF_FAILURE_RET(rc);

	TraceOSMemStack((uval64)(eInfo.stack.vaddr),
			(uval64)(eInfo.stack.memsz));

	Obj::ReleaseAccess(eInfo.localStackFR);
	eInfo.localStackFR.init();
	region[XferInfo::STACK].start = eInfo.stack.vaddr;
	region[XferInfo::STACK].size  = eInfo.stack.memsz;
	region[XferInfo::STACK].offset= 0;
	region[XferInfo::STACK].baseFROH.init();

    }


    return rc;
}

SysStatus
ProgExec::LoadAllModules(ProcessID newPID, XferInfo &xferInfo,
			 XHandle procXH, EntryPointDesc &entry)
{
    // Fooled you!!!!!!
    // This is called LoadAllModules, implying we load everything
    // for the new application, but that's not true.
    // We only load exec.so, it will load the application later on.

    // This is why we specify "entry" as an output, so that we can
    // swithc it to the exec.so entry point instead.

    MemRegion *region = &xferInfo.region[0];
    ExecInfo &eInfo = xferInfo.exec;

    SysStatus rc = 0;
    //
    // The child process needs access to the FR's in order to load
    // them itself, and to ensure they stay open until that occurs.
    //
    rc = Obj::GiveAccessByClient(eInfo.prog.localFR, eInfo.prog.frOH, newPID);
    _IF_FAILURE_RET(rc);

    if (eInfo.prog.interpOffset) {
	rc = Obj::GiveAccessByClient(eInfo.interp.localFR,
				     eInfo.interp.frOH, newPID);
	//FIXME: Clean up OH above on error
	//       How can we?  We don't own the OH!
	_IF_FAILURE_RET(rc);
    }

    ObjectHandle k42frOH;
    // Set up K42 library
    // The trivial "do" loop is there to let us break out of this sequence.
    do {
	ProgExec::BinInfo k42LibInfo;
	uval k42ImageStart;
	char * k42LibName = "/klib/exec.so";
	uval k42Origin = K42LIB_ADDR;
	uval k42LibOffset = 0;

	k42frOH.init();

	rc = RunProcessOpen(k42LibName, &k42ImageStart, &k42frOH, 0);

	// fixme cleanup needed
	if (_FAILURE(rc)) {
	    err_printf("No /klib/exec.so available. Can't load application\n");
	    break;
	}

	rc = ProgExec::ParseExecutable(k42ImageStart, k42frOH,&k42LibInfo);
	k42LibOffset =
	    k42Origin - k42LibInfo.seg[k42LibInfo.textSegIndex].vaddr;

	passertMsg(_SUCCESS(rc),"exec.so is an invalid ELF image\n");

	// relocate text and data of interpreter, and entry point, if needed
	passertMsg(k42LibOffset==0,"Cannot accept relocation of exec.so\n");

	// we enter app via the k42 library
	entry = k42LibInfo.entryPointDesc;

	rc = Obj::GiveAccessByClient(k42LibInfo.localFR, eInfo.k42FROH,
				     newPID);

	passertMsg(_SUCCESS(rc),"couldn't map exec.so: %lx\n",rc);

	k42LibInfo.frOH = eInfo.k42FROH;

	rc = ProgExec::MapModule(&k42LibInfo, procXH, region,
				 XferInfo::KTEXT, RegionType::KeepOnExec);


	region[XferInfo::KTEXT].baseFROH = eInfo.k42FROH;

	if (_FAILURE(rc)) break;

	rc = ConfigStack(&eInfo);
	tassertMsg(eInfo.localStackFR.valid(), "No stack defined\n");
	if (_FAILURE(rc)) break;


    } while (0);

    if (k42frOH.valid())
	Obj::ReleaseAccess(k42frOH);

    return rc;
}


/*static*/ void
ProgExec::ConsoleGiveAccess(ObjectHandle& ptyOH, ProcessID targetPID)
{
    /*
     * Processes started in user-land don't get access to the kernel Console
     * object.
     */
    ptyOH.init();
}

/*static*/ void
ProgExec::ProcessLinuxGiveAccess(ObjectHandle& procLinuxOH, ObjectHandle procOH)
{
    /*
     * Processes started in user-land are introduced to the ProcessLinux
     * server.
     */
    SysStatus rc;
    rc = DREFGOBJ(TheProcessLinuxRef)->createExecProcess(procOH, procLinuxOH);
    tassertMsg(_SUCCESS(rc), "ProcessLinux createExecProcess failed.\n");
}

/*static*/ SysStatus
ProgExec::CreateFirstDispatcher(ObjectHandle childOH,
			   EntryPointDesc entry, uval dispatcherAddr,
			   uval initMsgLength, char *initMsg)
{
    /*
     * In user-land, we can invoke the resource manager to create a dispatcher
     * in the child process.
     */
    return DREFGOBJ(TheResourceManagerRef)->
	    createFirstDispatcher(childOH, entry, dispatcherAddr,
				  initMsgLength, initMsg);
}


/*static*/ void
ProgExec::InitPhase2(uval initInfoArg)
{
    InitInfo& initInfo = *((InitInfo *) initInfoArg);
    XferInfo *const xferInfo = (XferInfo *) initInfo.xferHdr;

    DispatcherID const dspid = Scheduler::GetDspID();
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    /*
     * The IPC_RTN and IPC_FAULT entries must be initialized before we can
     * make any enabled PPC's.  The initialization can't be done before phase
     * 2 because a bogus return that arrives before the scheduler is ready
     * could be fatal.
     */
    Scheduler::EnableEntryPoint(IPC_RTN_ENTRY);
    Scheduler::EnableEntryPoint(IPC_FAULT_ENTRY);

    tassertWrn(((dspid == SysTypes::DSPID(0,0)) == (xferInfo != NULL)),
	       "xferInfo should be non-NULL only for dspid (0,0)\n");

    if (rd == 0) {
	BlockedThreadQueues::ClassInit(vp, &initInfo.memory);
    }

    MPMsgMgrDisabled::ClassInit(dspid, &initInfo.memory);

    if (rd == 0) {
	PageAllocatorUser::ClassInit(vp, &initInfo.memory);
    }

    DREFGOBJ(ThePageAllocatorRef)->deallocAllPages(&initInfo.memory);
    // initInfo.memory is empty now.

    /*
     * FIXME:
     * At this point we're done with the boot-time stack (including the
     * initInfo structure it contains).  We could deallocate it, except that
     * if we ever fork, the child will put its boot-time stack in this same
     * area, and we'd better be sure we haven't put something else there
     * in the meanwhile.  For now, just punt.  In the future we should maybe
     * invent a way to proactively give up the memory while hanging onto the
     * address space.  Tied up with this is the question about whether fork
     * can be called from VP's other than 0.  If we decide to do something,
     * the region in question is delimited by initInfo.stackStart and
     * initInfo.stackEnd.
     */

    DispatcherMgr::ClassInit(dspid);
    if (rd == 0) {
	XHandleTrans::ClassInit(vp);
    }

    if (dspid == SysTypes::DSPID(0,0)) {
	static StubTypeMgrServer stubSystemTypeMgr(StubObj::UNINITIALIZED);
	BackupTypeMgr = &stubSystemTypeMgr;
	stubSystemTypeMgr.setOH(xferInfo->typeServerObjectHandle);
    }

    if (rd == 0) {
	TypeMgr::ClassInit(vp, BackupTypeMgr);
    }

    if (rd == 0) {
        DREFGOBJ(TheCOSMgrRef)->vpMaplTransTablePaged(vp);
    }

    if (rd == 0 && SysTypes::DSPID(0,0)) {
	// Try to map a red zone at location zero.  If it fails that's ok
	// This is as early as we could have done it.
	StubRegionRedZone::_CreateFixedAddrLen(
	    0, PAGE_SIZE, 0,RegionType::UseSame+RegionType::KeepOnExec);
    }

    if (dspid == SysTypes::DSPID(0,0)) {
	SystemMiscWrapper::Create();
    }

#ifdef CLEANUP_DAEMON
    if (rd == 0) {
	// err_printf("\nStarting GC Daemon\n");
	((COSMgrObject *)DREFGOBJ(TheCOSMgrRef))->startPeriodicGC(vp);
    }
#endif /* #ifdef CLEANUP_DAEMON */

    if (dspid == SysTypes::DSPID(0,0)) {
       // jump to debugger if told to do so
	if (xferInfo && xferInfo->debugMe) {
	    GDBIO::SetDebugMe(xferInfo->debugMe);
#if defined(TARGET_powerpc)
	    // nothing to do for powerpc case
#elif defined(TARGET_mips64)
	    // PPC Simos does not require this
	    if (KernelInfo::OnSim()) {
		err_printf("preparing to debug.. touching all text pages\n");
		uval start = (uval)xferInfo->region[XferInfo::TEXT].start;
		uval size = start +
		    (uval)xferInfo->region[XferInfo::TEXT].size;

		for (uval p = start; p < size; p += PAGE_SIZE) {
		    DREFGOBJ(TheProcessRef)->userHandleFault(p, vp);
		}
	    }
#elif defined(TARGET_amd64)
    /* the mips SIMOS debugger does not work if it page faults, the powperpc does.
     * so the mips faults in all pages if the debugger is started.
     * for amd64 we will see if it is needed with SIMICS. XXX
     */
// #error Need TARGET_specific code TODO XXX pdb
#elif defined(TARGET_generic64)
	    // nothing to do for generic case
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

	}
    }

    if (rd == 0) {
	ProcessSetUser::ClassInit(vp);	// maintains info about processes
	ProcessWrapper::CreateKernelWrapper(vp);
    }

    if (dspid == SysTypes::DSPID(0,0)) {
	/*
	 * This routine initializes the ResMgrWrapper and ProcessLinuxClient
	 * classes.  The initial server has its own version in
	 * os/servers/baseServers/crtServer.C.  The default implementation
	 * for everyone else is in crtAux.C.
	 */
	InitCustomizationHook(xferInfo->processLinuxObjectHandle);
    }

    /*
     * The user-mode debugger should work now.
     */
    Scheduler::EnableEntryPoint(TRAP_ENTRY);

    if (dspid == SysTypes::DSPID(0,0)) {
	FileLinux::ClassInit();
	MountPointMgrClient::ClassInit(vp);

	memoryMapCheck();
    }

    if (dspid == SysTypes::DSPID(0,0)) {
	SchedulerService::ClassInit(xferInfo->userProcessID);
    }

    if (dspid == SysTypes::DSPID(0,0)) {
	// Set umask
	(void) FileLinux::SetUMask(xferInfo->umask);

	// If the parents cwd is NULL then we were created by the kernel
	if (xferInfo->cwd != NULL) {
	    // Set Current Working Directory
	    // If Chdir fails or is garbage then it will default to root.
	    SysStatus rc = FileLinux::Chdir(xferInfo->cwd);
	    tassertWrn(_SUCCESS(rc), "Chdir to %s failed\n",
		       xferInfo->cwd);
	}
    }

    /*
     * Now we're ready for incoming work requests.
     * No primitive memory manager available at this point.
     */
    MPMsgMgrEnabled::ClassInit(dspid, NULL);

    if (rd==0) {
	MemTrans::ClassInit(vp);
	ShMemClnt::ClassInit(vp);
	ShMemBuf::ClassInit(vp);
    }

    /*
     * Now we're ready for reflected system calls.
     */
    Scheduler::EnableEntryPoint(SVC_ENTRY);

    /*
     * Now we're ready for incoming PPC's.
     */
    Scheduler::EnableEntryPoint(IPC_CALL_ENTRY);

    if (dspid != SysTypes::DSPID(0,0)) {
	/*
	 * Note:  For all but the very first dispatcher, ForkLock was acquired
	 *        in CreateDispatcher.  We release it here now that the new
	 *        dispatcher is fully operational.
	 */
	_ASSERT_HELD_PTR(ProgExec_ForkLock);
	ProgExec_ForkLock->release();
	return;
    }

    LinuxLibInit();

    // The FR OH's in the exec structure are now local
    ExecXferInfo->exec.localize();

    // Run the program
    RunProg(ExecXferInfo);
}

/*****************************************************************************
 *Note: it is not safe to acquire any form of blocking locks until the entire
 *      dispatcher including its MPMsgMgrs have been initialized, which
 *      only occurs in InitPhase2.  This means until then all ClassInit
 *      methods must not use any form of blocking locks.
 * FIXME:  Find some way of reliably asserting if the rule is violated.
 *****************************************************************************/

/*static*/ void
ProgExec::Init(EntryPointDesc *entry)
{
    //
    //
    // We have to clear BSS still don't touch BSS data until we have done so
    //
    //

    // FIXME: do real per-processor initialization

    Dispatcher *const dispatcher = extRegsLocal.dispatcher;
    DispatcherID const dspid = dispatcher->getDspID();
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    SysStatus rc;
    ProcessID badge;
    XHandle xh;
    uval method;
    uval length;
    uval msgBuf[AsyncBuffer::MSG_LENGTH];

    rc = dispatcher->asyncBufferLocal.fetchMsg(badge, xh, method,
					       length, msgBuf);

    tassertSilent(_SUCCESS(rc), BREAKPOINT);
    tassertSilent(badge == _KERNEL_PID, BREAKPOINT);
    tassertSilent(xh == 0, BREAKPOINT);
    tassertSilent(method == 0, BREAKPOINT);
    tassertSilent(length >= sizeof(InitMsg), BREAKPOINT);

    InitMsg *const initMsg = (InitMsg *) msgBuf;

    tassertSilent(initMsg->dispatcherStart == uval(dispatcher), BREAKPOINT);

    /*
     * Space reserved for the dispatcher and for our current stack is at the
     * front of the dispatcher area.  All the rest of the dispatcher area is
     * available for dynamic allocation.
     */
    uval allocStart, allocEnd;
    XferInfo *const xferInfo = (XferInfo *) initMsg->xferHdr;
    allocStart = initMsg->dispatcherStart + Scheduler::DISPATCHER_SPACE;
    allocEnd = initMsg->dispatcherStart + initMsg->dispatcherSize;

    tassertSilent(PAGE_ROUND_DOWN(allocStart) == allocStart, BREAKPOINT);
    tassertSilent(PAGE_ROUND_DOWN(allocEnd)   == allocEnd,   BREAKPOINT);

    if (dspid == SysTypes::DSPID(0,0)) {
	/*
	 * Clear the part of the last page of data region which
	 * is really bss.  Do this for any currently loaded module.
	 */
	for (int i = 0; i<XferInfo::NUMREGIONS; ++i) {
	    switch (i) {
	    case XferInfo::TEXT:
	    case XferInfo::ITEXT:
	    case XferInfo::KTEXT:
		if (xferInfo->region[i].size) {
		    memset((void*)(xferInfo->region[i+1].start+
				   xferInfo->region[i+1].size),
			   0,
			   xferInfo->region[i+2].start-(
			       xferInfo->region[i+1].start+
			       xferInfo->region[i+1].size));
		}
		break;
	    }
	}

	// We're safe to touch BSS (ProgExec::* static members)

	ProgExec_ForkLock->init();
    }

    // Save xferInfo pointer
    if (xferInfo) {
	ExecXferInfo = xferInfo;
    }

    // Intialize some values, including setting up to allow disabled PPC.
    Scheduler::Init();

    if (dspid == SysTypes::DSPID(0,0)) {
	// Store the process ID and the name of the program in global
	// variables where other VPs can find them.
	PID = xferInfo->userProcessID;
    }

    // Store the process ID and the name of the program in the dispatcher
    // so that when we stop in the debugger we know what program is running.
    Scheduler::StoreProgInfo(PID, ExecXferInfo->progName);

    // Allocate a InitInfo structure on the stack.
    InitInfo initInfo;

    // Put the remaining space into a primitive memory manager.
    initInfo.memory.init(allocStart, allocEnd, KernelInfo::SCacheLineSize());

    // Record the bounds of the transfer area (only used on vp 0).
    initInfo.xferStart = initMsg->xferStart;
    initInfo.xferSize = initMsg->xferSize;
    initInfo.xferHdr = initMsg->xferHdr;

    if (rd == 0) {
	ActiveThrdCnt::ClassInit(vp);
	BaseObj::ClassInit(vp);
	COSMgrObject::ClassInit(vp, &initInfo.memory);

	if (vp == 0) {
	    ProcessWrapper::InitMyProcess(vp,
					  xferInfo->processObjectHandle,
					  xferInfo->userProcessID,
					  &initInfo.memory);
	} else {
	    ObjectHandle oh;
	    oh.initWithPID(0, 0);
	    ProcessWrapper::InitMyProcess(vp, oh, 0, NULL);
	}
    }

    /*
     * The RUN, INTERRUPT, and PGFLT entries must be initialized before we
     * get into InitPhase2.
     */
    Scheduler::EnableEntryPoint(RUN_ENTRY);
    Scheduler::EnableEntryPoint(INTERRUPT_ENTRY);
    Scheduler::EnableEntryPoint(PGFLT_ENTRY);

    /*
     * Initialize the scheduler; this execution context becomes a thread.
     */

    Thread *thread;
    thread = ProcessLinuxClient::AllocThread(&initInfo.memory);
    Scheduler::ClassInit(dspid, thread, &initInfo.memory,
			 THREAD_COUNT, THREAD_SIZE, THREAD_STACK_RESERVATION,
			 InitPhase2, uval(&initInfo));
    /* NOTREACHED */
}

/*
 * FIXME: we are currently not protecting against files being closed or
 * opened during a fork, i.e., in a multi-threaded application.  If this
 * happens, death will follow.  We need to look at the mechanisms in glibc
 * for pthread implementations, and think how to lock for proper fork
 * implementation.
 */
/*static*/ void
ProgExec::ForkWorker(uval forkInfoArg)
{
    ForkInfo& forkInfo = *((ForkInfo *) forkInfoArg);
    SysStatus rc;
    ObjectHandle childProcessOH;
    StubProcessServer childProcStub(StubObj::UNINITIALIZED);
    DispatcherID dspid;

    dspid = Scheduler::GetDspID();
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    tassert(vp == 0, err_printf("Not on VP 0!\n"));
    _ASSERT_HELD_PTR(ProgExec_ForkLock);

    // pass the parent pid
    //FIXME: when we have process relationships this should go away
    forkInfo.parentPID = _SGETPID(DREFGOBJ(TheProcessRef)->getPID());

    forkInfo.rc = StubProcessServer::_Create(childProcessOH, PROCESS_DEFAULT,
					     ExecXferInfo->progName,
					     forkInfo.childPID);
    if (_FAILURE(forkInfo.rc)) goto leave;

    forkInfo.childProcessXH = childProcessOH.xhandle();

    // must be done before we preFork memory but after we know who the
    // child is.
    if (IOForkManager::IsInitialized()) {
	forkInfo.rc = IOForkManager::PreFork(forkInfo.childProcessXH);
	if (_FAILURE(forkInfo.rc)) goto leave;
    }

    childProcStub.setOH(childProcessOH);
    forkInfo.rc = childProcStub._giveAccess(forkInfo.childOH,
					    forkInfo.childPID);
    if (_FAILURE(forkInfo.rc)) goto leave;

    //pass in the child process OH, get back a ProcessLinux object handle and
    //linux PID for the child, and prepare for postFork call to work.
    forkInfo.rc = DREFGOBJ(TheProcessLinuxRef)->
					preFork(childProcessOH,
						forkInfo.childProcessLinuxOH,
						forkInfo.childLinuxPID);
    if (_FAILURE(forkInfo.rc)) goto leave;

    ((COSMgrObject *)DREFGOBJ(TheCOSMgrRef))->pausePeriodicGC();

    DispatcherID lastDspid;
    RDNum lastRD;
    Dispatcher *lastDsp;

    lastRD = Scheduler::RDLimit;
    do {
	tassertMsg(lastRD > 0, "No dispatchers on this VP!\n");
	lastRD--;
	lastDspid = SysTypes::DSPID(lastRD, vp);
	rc = DREFGOBJ(TheDispatcherMgrRef)->getDispatcher(lastDspid, lastDsp);
	tassertMsg(_SUCCESS(rc), "getDispatcher failed.\n");
    } while (lastDsp == NULL);

    if (dspid == lastDspid) {
	forkInfo.rc = ForkAddressSpace(forkInfoArg);
    } else {
	Scheduler::DeactivateSelf();
	forkInfo.rc = SchedulerService::CallFunction(lastDspid,
						     ForkAddressSpace,
						     forkInfoArg);
	Scheduler::ActivateSelf();
    }

    if (_FAILURE(forkInfo.rc)) {
	passertMsg(0, "Must clean up child process.\n");
	goto leave;
    }

    //Note that in the fork child, GC is restarted from scratch, so
    //the fact that we copied memory while it was stopped is ok
    ((COSMgrObject *)DREFGOBJ(TheCOSMgrRef))->unpausePeriodicGC();

    Dispatcher *dsp;
    rc = DREFGOBJ(TheDispatcherMgrRef)->
			getDispatcher(SysTypes::DSPID(0,0), dsp);
    tassertMsg(_SUCCESS(rc), "getDispatcher failed.\n");

    InitMsg initMsg;
    initMsg.dispatcherStart = uval(dsp);
    initMsg.dispatcherSize = Scheduler::DISPATCHER_SPACE;
    initMsg.xferHdr = initMsg.xferStart = uval(&forkInfo);
    initMsg.xferSize = sizeof(ForkInfo);

    forkInfo.rc = DREFGOBJ(TheResourceManagerRef)->
		    createFirstDispatcher(childProcessOH, ForkChildDesc,
					  initMsg.dispatcherStart,
					  sizeof(initMsg), (char *)(&initMsg));
    if (_FAILURE(forkInfo.rc)) {
	passertMsg(0, "Must clean up child process.\n");
	goto leave;
    }

    // Parent has no further need of child process OH.
    (void) childProcStub._releaseAccess();

leave:
    // Restart parent
    Scheduler::ResumeThread(forkInfo.forkThread);
}

/*static*/ SysStatus
ProgExec::ForkAddressSpace(uval forkInfoArg)
{
    ForkInfo& forkInfo = *((ForkInfo *) forkInfoArg);
    SysStatus rc;
    DispatcherID dspid;

    dspid = Scheduler::GetDspID();
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    Scheduler::DeactivateSelf();
    Scheduler::Disable();
    if (ActiveThrdCnt::AnyActive()) {
	// Some other thread is doing possibly bad things.  Yield() once, and
	// if that doesn't do it, Delay() so that other dispatchers can run.
	Scheduler::DisabledYield();
	while (ActiveThrdCnt::AnyActive()) {
	    Scheduler::Enable();
	    Scheduler::DelayMicrosecs(1000);
	    Scheduler::Disable();
	}
    }

    ALLOW_PRIMITIVE_PPC();

    if (rd > 0) {
	DispatcherID nextDspid;
	RDNum nextRD;
	Dispatcher *nextDsp;

	nextRD = rd;
	do {
	    tassertMsg(nextRD > 0, "No more dispatchers on this VP!\n");
	    nextRD--;
	    nextDspid = SysTypes::DSPID(nextRD, vp);
	    rc = DREFGOBJ(TheDispatcherMgrRef)->getDispatcher(nextDspid,
							      nextDsp);
	    tassertMsg(_SUCCESS(rc), "getDispatcher failed.\n");
	} while (nextDsp == NULL);

	rc = SchedulerService::CallFunction(nextDspid,
					    ForkAddressSpace, forkInfoArg);
    } else {
	Scheduler::DisabledPreFork(forkInfo.schedForkInfo);
	rc = DREFGOBJ(TheProcessRef)->preFork(forkInfo.childProcessXH);
    }

    UN_ALLOW_PRIMITIVE_PPC();

    Scheduler::Enable();
    Scheduler::ActivateSelf();

    return rc;
}

/*static*/ void
ProgExec::ForkChildPhase2(uval forkInfoArg)
{
    ForkInfo& forkInfo = *((ForkInfo *) forkInfoArg);

    tassertMsg(Scheduler::GetDspID() == SysTypes::DSPID(0,0),
	       "Wrong dispatcher.\n");
    _ASSERT_HELD_PTR(ProgExec_ForkLock);

    /*
     * The IPC_RTN and IPC_FAULT entries must be initialized before we can
     * make any enabled PPC's.  The initialization can't be done before phase
     * 2 because a bogus return that arrives before the scheduler is ready
     * could be fatal.
     */
    Scheduler::EnableEntryPoint(IPC_RTN_ENTRY);
    Scheduler::EnableEntryPoint(IPC_FAULT_ENTRY);

    ((ProcessWrapper*)(DREFGOBJ(TheProcessRef)))->postFork(forkInfo.childPID);

    DREFGOBJ(TheBlockedThreadQueuesRef)->postFork();

    /*
     * The user-mode debugger should work now.
     */
    GDBIO::PostFork();
    Scheduler::EnableEntryPoint(TRAP_ENTRY);

    MPMsgMgrDisabled::PostFork();

    DREFGOBJ(TheDispatcherMgrRef)->postFork();

    ((COSMgrObject *)DREFGOBJ(TheCOSMgrRef))->postFork();

    DREFGOBJ(TheProcessLinuxRef)->postFork(forkInfo.childProcessLinuxOH,
					   forkInfo.childLinuxPID,
					   forkInfo.forkThread);

    DREFGOBJ(TheResourceManagerRef)->postFork();

    ShMemClnt::PostFork();
    ShMemBuf::PostFork();

    SchedulerService::PostFork(forkInfo.childPID);

    // Store the process ID and the name of the program in the dispatcher
    // so that when we stop in the debugger we know what program is running.
    PID = forkInfo.childPID;
    Scheduler::StoreProgInfo(PID, ExecXferInfo->progName);

    // Now we're ready for incoming work requests.
    MPMsgMgrEnabled::PostFork();

    // Now we're ready for reflected system calls.
    Scheduler::EnableEntryPoint(SVC_ENTRY);

    // Now we're ready for incoming PPC's.
    Scheduler::EnableEntryPoint(IPC_CALL_ENTRY);

    // Indicate that we're the child.
    forkInfo.childLinuxPID = 0;

    // Restart forkThread.
    Scheduler::AddThread(forkInfo.forkThread);
}

/*
 * the forked child starts executing here.  Its start message tells it
 * where forkInfo is.  most of the memory is already correct.  work to
 * be done is to re-init the dispatcher, tell the dispatcher about the
 * one thread that will be running in the child.  all objects which
 * need to reset themselves because of the fork need to be called.
 */
/*static*/ void
ProgExec::ForkChild()
{
    Dispatcher *const dispatcher = extRegsLocal.dispatcher;
    DispatcherID const dspid = dispatcher->getDspID();

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    tassert(vp == 0, err_printf("Wrong VP.\n"));
    _ASSERT_HELD_PTR(ProgExec_ForkLock);

    // Intialize some values, including setting up to allow disabled PPC.
    Scheduler::Init();

    SysStatus rc;
    ProcessID badge;
    XHandle xh;
    uval method;
    uval length;
    uval msgBuf[AsyncBuffer::MSG_LENGTH];

    rc = dispatcher->asyncBufferLocal.fetchMsg(badge, xh, method,
					       length, msgBuf);
    tassertSilent(_SUCCESS(rc), BREAKPOINT);
    tassertSilent(badge == _KERNEL_PID, BREAKPOINT);
    tassertSilent(xh == 0, BREAKPOINT);
    tassertSilent(method == 0, BREAKPOINT);
    tassertSilent(length >= sizeof(InitMsg), BREAKPOINT);

    InitMsg *const initMsg = (InitMsg *) msgBuf;

    tassertSilent(initMsg->dispatcherStart == uval(dispatcher), BREAKPOINT);

    ForkInfo& forkInfo = *((ForkInfo *) (initMsg->xferHdr));

    if (rd == 0) {
	ActiveThrdCnt::ClassInit(vp);
	/*
	 * We can't call postFork() on the wrapper until phase 2 because of
	 * locking assertions, but we need the correct object handle in the
	 * wrapper so that we can make the EnableEntryPoint() calls below.
	 */
	((ProcessWrapper*)(DREFGOBJ(TheProcessRef)))->setOH(forkInfo.childOH);
    }

    /*
     * The RUN, INTERRUPT, and PGFLT entries must be initialized before we
     * get into ForkChildPhase2.
     */
    Scheduler::EnableEntryPoint(RUN_ENTRY);
    Scheduler::EnableEntryPoint(INTERRUPT_ENTRY);
    Scheduler::EnableEntryPoint(PGFLT_ENTRY);

    /*
     * Initialize the scheduler and continue on a thread in ForkChildPhase2().
     */
    Scheduler::DisabledPostFork(forkInfo.schedForkInfo,
				ForkChildPhase2, uval(&forkInfo));
    /* NOTREACHED */
}

/*static*/ SysStatus
ProgExec::ForkProcess(pid_t& childLinuxPID)
{
    AutoLock<StaticLock> al(ProgExec_ForkLock); // locks now, unlocks on return
				     		//   (in both parent and child)

    passertMsg(!MultiVP, "Can't fork a multi-vp program.\n");

    tassertMsg(Scheduler::IsForkSafeSelf(),
	       "Fork thread isn't safe from reclamation in the child.\n");

    ForkInfo forkInfo;
    Scheduler::AllocForkInfo(forkInfo.schedForkInfo);

    forkInfo.forkThread = Scheduler::GetCurThreadPtr();
    forkStack = (uval)allocPinnedGlobalPadded(THREAD_SIZE);

    /*
     * We have to make sure the current thread really blocks before the
     * ForkWorker thread starts, because the fork child is going to unblock
     * its copy of the current thread.  If the thread happens to be on the
     * ready queue when the copy is made, it will be lost in the child.
     * We run disabled to achieve the necessary atomicity.
     */
    Scheduler::DeactivateSelf();
    Scheduler::Disable();
    Scheduler::DisabledScheduleFunction(ForkWorker, uval(&forkInfo));
    Scheduler::DisabledSuspend();
    Scheduler::Enable();
    Scheduler::ActivateSelf();

    Scheduler::DeallocForkInfo(forkInfo.schedForkInfo);

    childLinuxPID = forkInfo.childLinuxPID;

    if (childLinuxPID != 0) {
	// am parent
	freePinnedGlobalPadded((void*)forkStack, THREAD_SIZE);
    } else {
	// am child
	switch (GDBIO::GetDebugMe()) {
	case GDBIO::NO_DEBUG:
	    break;
	case GDBIO::USER_DEBUG:
	    BREAKPOINT;
	    break;
	default:
	    breakpoint();
	    break;
	}

#ifndef NDEBUG
	uval envp = (uval)(ExecXferInfo->envp);
	char* target = NULL;
	char* envstring;
	while ((envstring = (char*)(Is32Bit()?*(uval32*)envp:*(uval*)envp))) {
	    if (memcmp(envstring,"DEBUGPROC",9)==0) {
		target = envstring;
		break;
	    }
	    envp += Is32Bit()?4:8;
	}
	if (target) {
	    target += 10;
	    if (strstr(extRegsLocal.dispatcher->progName, target)) {
		breakpoint();
	    }
	}
#endif // NDEBUG
    }
    return forkInfo.rc;
}


/*static*/ SysStatus
ProgExec::CreateVP(VPNum vp)
{
    SysStatus rc;

    rc = CreateDispatcher(SysTypes::DSPID(0, vp));
    _IF_FAILURE_RET(rc);

    MultiVP = 1;	// to inhibit fork()

    return 0;
}


struct ProgExec::CreateDispatcherMsg : MPMsgMgr::MsgSync {
    DispatcherID dspid;
    SysStatus rc;

    virtual void handle() {
	rc = CreateDispatcher(dspid);
	reply();
    }
};

/*static*/ SysStatus
ProgExec::CreateDispatcher(DispatcherID dspid)
{
    SysStatus rc;
    uval dispatcherAddr;

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if ((rd != 0) && (vp != Scheduler::GetVP())) {
	/*
	 * We'll ask the resource manager to make the remote requests necessary
	 * to create the first dispatcher on a new vp.  For all other
	 * dispatchers on that vp we ship the request to the first dispatcher,
	 * mainly as a synchronization measure.  We want the first dispatcher
	 * to be fully initialized before we try to run others.
	 */
	MPMsgMgr::MsgSpace msgSpace;
	CreateDispatcherMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), msgSpace) CreateDispatcherMsg;
	tassert(msg != NULL, err_printf("message allocate failed.\n"));

	msg->dspid = dspid;

	rc = msg->send(SysTypes::DSPID(0, vp));
	tassert(_SUCCESS(rc), err_printf("send failed\n"));

	return msg->rc;
    }

    /*
     * Note:  ForkLock is acquired here and released at the end of InitPhase2
     *        in the new dispatcher.  It keeps new dispatchers from appearing
     *        while ForkAddressSpace is trying to quiesce all dispatchers.
     */
    ProgExec_ForkLock->acquire();

    uval node, d1;
    rc = DREFGOBJ(ThePageAllocatorRef)->getNumaInfo(vp, node, d1);

    rc = DREFGOBJ(ThePageAllocatorRef)->
	    bindRegionToNode(node, INIT_MEM_SIZE, dispatcherAddr);
    _IF_FAILURE_RET(rc);

    // We claim the last THREAD_SIZE of INIT_MEM_SIZE for the initial stack
    // entry.S will claim the last THREAD_SIZE for use as a stack
    InitMsg initMsg;
    initMsg.dispatcherStart = dispatcherAddr;
    initMsg.dispatcherSize = INIT_MEM_SIZE - THREAD_SIZE;
    initMsg.xferStart = 0;	// no transfer area for secondary vp's
    initMsg.xferSize = 0;
    initMsg.xferHdr = 0;

    rc = DREFGOBJ(TheResourceManagerRef)->
	    createDispatcher(dspid, SecondaryStartDesc, dispatcherAddr,
			     sizeof(initMsg), (char *)(&initMsg));
    if (_FAILURE(rc)) {
	SysStatus tmpRC;
	tmpRC = DREFGOBJ(ThePageAllocatorRef)->
		    deallocPages(dispatcherAddr, INIT_MEM_SIZE);
	tassertMsg(_SUCCESS(tmpRC), "Dispatcher dealloc failed.\n");
    }

    return rc;
}

/*static*/ void
ProgExec::UnMapAndExec(ArgDesc *args)
{
    SysStatus rc = LinuxPreExec();
    passertMsg(_SUCCESS(rc), "LinuxPreExec: %lx\n",rc);

    ProgExec::LoadAndRun(args->prog, args);
}

// look for kitch-linux/lib/emu/readlink.C
extern char __readlink_proc_self_exe_hack[128];

//FIXME MAA why isn't atoi or baseAtoi available when I build baseservers?
static uval
myAtoi(const char *nptr)
{
	const char *s = nptr;
	int c;
	unsigned long cutoff;
	int neg = 0, any, cutlim;
	int base = 0;
	uval acc;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do {
		c = *s++;
	} while (c == ' ');
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
	cutlim = cutoff % (unsigned long)base;
	cutoff /= (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'F')
			c -= 'A' - 10;
		else if (c >= 'a' && c <= 'a')
			c -= 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff ||
		    ((acc == cutoff) && (c > cutlim)))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (neg)
		acc = -acc;
	return acc;
}

/*static*/ void
ProgExec::LoadAndRun(ProgExec::ExecInfo &prog, ArgDesc* args)
{
    char **envp;
    SysStatus rc;
    EntryPointDesc entry;

    ExecXferInfo->region[XferInfo::TEXT].size = 0;
    ExecXferInfo->region[XferInfo::ITEXT].size = 0;
    ExecXferInfo->region[XferInfo::DATA].size = 0;
    ExecXferInfo->region[XferInfo::IDATA].size = 0;
    ExecXferInfo->region[XferInfo::STACK].size = 0;

    /*
     * We have to look for the environment variable for large data segments
     * early, before loading the program.
     */
    if (args != NULL) {
	for (envp = args->getEnvp(); *envp != NULL; envp++) {
	    if (strncmp(*envp, "K42_LARGE_DATA_SEG", 18) == 0) {
		ExecXferInfo->region[XferInfo::DATA].pageSize =
							myAtoi((*envp) + 19);
		break;
	    }
	}
    }

    rc = DREFGOBJ(TheResourceManagerRef)->execNotify();

    rc = LoadBinary(prog, &ExecXferInfo->region[0], &entry);
    tassertMsg(_SUCCESS(rc), "LoadBinary: %lx\n", rc);

    memcpy(&ExecXferInfo->exec, &prog, sizeof(prog));

    uval argEnd = prog.stack.vaddr + prog.stack.memsz;
    uval argStart = prog.stack.vaddr;
    if (args) {

	rc = SetupStack(argStart, argEnd, argEnd, ExecXferInfo, args);

	tassertMsg(_SUCCESS(rc), "SetupStack: %lx\n", rc);

	strncpy(ExecXferInfo->progName, args->getArgv0(), 64);
	ExecXferInfo->progName[63] = '\0';

	strncpy(__readlink_proc_self_exe_hack, args->getFileName(), 128);
	__readlink_proc_self_exe_hack[127] = '\0';

	args->destroy();

    } else {
	argEnd = uval(&ExecXferInfo->argv[0]) - (Is32Bit() ? 4 : 8);
	char *argv0 = Is32Bit() ?
			((char *) uval(*((uval32 *) ExecXferInfo->argv))) :
			ExecXferInfo->argv[0];
	strncpy(ExecXferInfo->progName, argv0, 64);
	ExecXferInfo->progName[63] = '\0';
	strncpy(__readlink_proc_self_exe_hack, argv0, 128);
	__readlink_proc_self_exe_hack[127] = '\0';
    }

    if (args != NULL) {
	for (envp = args->getEnvp(); *envp != NULL; envp++) {
	    if (strncmp(*envp, "K42_LARGE_BRK_HEAP", 18) == 0) {
		ExecXferInfo->largeBrkHeap = myAtoi((*envp) + 19);
		break;
	    }
	}
    }

#ifndef NDEBUG
    // if DEBUGPROC matches progName, breakpoint
    if (args != NULL) {
	for (envp = args->getEnvp(); *envp != NULL; envp++) {
	    if (strncmp(*envp, "DEBUGPROC", 9) == 0) {
		if (strstr(ExecXferInfo->progName, (*envp) + 10)) {
		    breakpoint();
		}
		break;
	    }
	}
    }

    // if TRACEPROC matches progName, trace syscalls
    if (args != NULL) {
	for (envp = args->getEnvp(); *envp != NULL; envp++) {
	    if (strncmp(*envp, "TRACEPROC", 9) == 0) {
		if (strstr(ExecXferInfo->progName, (*envp) + 10)) {
		    extern void traceLinuxSyscalls();
		    traceLinuxSyscalls();
		}
		break;
	    }
	}
    }
#endif // NDEBUG

    // PID doesn't change on exec, but progName does.  We have to update
    // all existing dispatchers, not just our own.
    rc = DREFGOBJ(TheDispatcherMgrRef)->
			    publishProgInfo(PID, ExecXferInfo->progName);
    tassertMsg(_SUCCESS(rc), "publishProgInfo failed.\n");

    VolatileState vs;
    NonvolatileState nvs;
    nvs.init();
    vs.init(argEnd, &entry, Is32Bit());
    Scheduler::LaunchSandbox(&vs, &nvs);
    // NOTREACHED
}

extern "C" void _start(void) __attribute__((weak));
void
ProgExec::RunProg(ProgExec::XferInfo *info)
{
    if (_start==0) {
	// No args, stack has already been set up by parent
	LoadAndRun(info->exec, NULL);
	// NOTREACHED
    } else {
	// argc is below argv on stack, which is what stack pointer must
	// be set to
	SysStatus rc = StubRegionDefault::_CreateFixedAddrLenExt(
				info->exec.stack.vaddr,
				info->exec.stack.memsz,
				info->exec.localStackFR, 0,
				(uval)(AccessMode::writeUserWriteSup),
				0, RegionType::ForkCopy);

	tassertMsg(_SUCCESS(rc),"Stack mapping: %lx\n", rc);

	TraceOSMemStack((uval64)info->exec.stack.vaddr,
			(uval64)info->exec.stack.memsz);

	Obj::ReleaseAccess(info->exec.localStackFR);
	info->exec.stackFR.init();
	info->region[XferInfo::STACK].start = info->exec.stack.vaddr;
	info->region[XferInfo::STACK].size  = info->exec.stack.memsz;
	info->region[XferInfo::STACK].offset= 0;
	info->region[XferInfo::STACK].baseFROH.init();

	VolatileState vs;
	NonvolatileState nvs;
	nvs.init();
	vs.init(uval(info->argv) - 8,
		(EntryPointDesc *) _start,
		/*is32Bit*/ 0);
	Scheduler::LaunchSandbox(&vs, &nvs);
	// NOTREACHED
    }
}

void
ProgExec_Init()
{
    ProgExec::Init();
};

void
ProgExec_ForkChild()
{
    ProgExec::ForkChild();
}


void
ProgExec::ExecInfo::localize()
{
    localStackFR = stackFR;
    prog.localFR = prog.frOH;
    interp.localFR = interp.frOH;
}
