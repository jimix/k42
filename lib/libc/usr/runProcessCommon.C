/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: runProcessCommon.C,v 1.167 2005/04/15 17:39:36 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: establish and call a user process
 *  todo:
 *    - consider moving all this functionality into a create static function
 *      of process wrapper
 *    - have a separate structure used for initailizng/passing I/O objects,
 *      since in the long run we will be starting children using different
 *      objects from parents
 *    - pass along environment variables as an explicit argument, so different
 *      ones can be passed to child
 *    - get rid of all refs in kernel passed through, i.e., security model,
 *      factory objects...
 *    - change target process to a process ref that return, block in calling
 *      function
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ProcessServer.H>
#include <stub/StubProcessServer.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubRegionPerProcessor.H>
#include <stub/StubRegionRedZone.H>
#include <stub/StubFRComputation.H>
#include <stub/StubFRCRW.H>
#include <stub/StubObj.H>
#include <cobj/TypeMgr.H>
#include <alloc/PageAllocatorUser.H>
#include "cobj/XHandleTrans.H"
#include <sys/Dispatcher.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <mem/Access.H>
#include "io/PathName.H"
#include <sys/memoryMap.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <io/IOForkManager.H>
#include <usr/ProgExec.H>
#include "runProcessCommon.H"
#include <sys/ProcessLinux.H>
#include <usr/GDBIO.H>
#include <sys/ResMgrWrapper.H>
#include <defines/experimental.H>
#include <sys/KernelInfo.H>
#include <elf.h>

uval useLongExec = 0;
// In a shared library, ld.so will call this function upon linking.
// We use this to track if an application has linked against libk42sys.so,
// and is thus a K42 app which must use the slow exec process
extern "C" void _init() __attribute__((weak));
void _init()
{
    useLongExec = 1;
}

SysStatusProcessID
RunProcessCommon(const char *name, uval imageStart, ObjectHandle imageFROH,
		 ProgExec::ArgDesc *args, char const *cwd,
		 ProcessID myPID, uval wait)
{
    SysStatus rc;
    EntryPointDesc entry;
    ProgExec::XferInfo initialXferInfo;	// local until we alloc workspace
    uval workSpaceStart = K42WSPACE_ADDR;
    uval workSpaceStartLocal = 0;
    uval stackBottomLocal = 0;
    uval stackTop;
    uval stackTopLocal;

// fake loop to allow for abort
do {

    memset(&initialXferInfo, 0, sizeof(ProgExec::XferInfo));

// FIXME FIXXME descriptify FSOH
// FIXME FIXXME check to if imageOH is being destroyed, esp from kernel
// FIXME FIXXME instead of creating region from both kernel and user
// FIXXME just do it here, but make sure to have already given FRPlaceHolder
// FIXXME the correct size when coming frmo the kernel

    rc = ProgExec::ParseExecutable(imageStart, imageFROH,
				   &initialXferInfo.exec.prog);
    if (_FAILURE(rc)) break;

    initialXferInfo.exec.entry = initialXferInfo.exec.prog.entryPointDesc;

    if (initialXferInfo.exec.prog.interpOffset) {
	uval interpName = imageStart + initialXferInfo.exec.prog.interpOffset;
	uval interpreterImageStart;
	ObjectHandle frOH;
	rc = RunProcessOpen((const char*)interpName, &interpreterImageStart,
			    &frOH, 0);
	if (_FAILURE(rc)) break;

	rc = ProgExec::ParseExecutable(interpreterImageStart, frOH,
				       &initialXferInfo.exec.interp);

	if (_FAILURE(rc)) break;
	initialXferInfo.exec.entry= initialXferInfo.exec.interp.entryPointDesc;
    }

    XHandle targetXH;
    ProcessID targetPID;
    struct ProcessHolder {
	StubProcessServer st;

	ProcessHolder(ProcessID &id, const char *nm) :
	    st(StubObj::UNINITIALIZED) {
	    ObjectHandle oh;
	    SysStatus rc;
	    rc = StubProcessServer::_Create(oh, PROCESS_DEFAULT, nm, id);
	    tassert( _SUCCESS(rc), err_printf("woops\n"));
	    st.setOH(oh);
	};
	DEFINE_GLOBAL_NEW(ProcessHolder);
    } uProc(targetPID, name);

    // this has to succeed
    targetXH = uProc.st.getOH().xhandle();

    TraceOSUserSpawn(targetPID, myPID, 0, name);

    // FIXME: change to request from my typeserver handle of kernel type server
    ObjectHandle tsrv;
    DREFGOBJ(TheTypeMgrRef)->getTypeMgrOH(tsrv);

    // create region for work space, intialize xfer info
    ObjectHandle workSpaceFROH;
    ObjectHandle childWorkSpaceFROH;


    rc = StubFRComputation::_Create(workSpaceFROH);

    if (_FAILURE(rc)) break;

    // Force alignment on segment boundary, so that assembly entry code
    // can locate the bottom of the stack.
    // Stack is within work space, outside of xferInfo
    rc = StubRegionDefault::_CreateFixedAddrLenExt(
	workSpaceStart, ProgExec::WORKSPACE_SIZE,
	workSpaceFROH, 0, (uval)(AccessMode::writeUserWriteSup), targetXH,
	RegionType::ForkCopy+RegionType::KeepOnExec);

    if (_FAILURE(rc)) break;

    // bind xfer region in to my own address space for initialization
    // Note that we map in xfer area after leaving space for stack.
    rc = StubRegionDefault::_CreateFixedLenExt(
	workSpaceStartLocal, ProgExec::XFERINFO_MAX_SIZE, 0,
	workSpaceFROH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);

    if (_FAILURE(rc)) break;

    // passed to child below
    Obj::GiveAccessByClient(workSpaceFROH, childWorkSpaceFROH, targetPID);

    Obj::ReleaseAccess(workSpaceFROH);

    if (_FAILURE(rc)) break;

    initialXferInfo.region[ProgExec::XferInfo::WORKSPACE].start =
	workSpaceStart;

    initialXferInfo.region[ProgExec::XferInfo::WORKSPACE].size =
	ProgExec::WORKSPACE_SIZE;

    initialXferInfo.region[ProgExec::XferInfo::WORKSPACE].offset = 0;

    initialXferInfo.region[ProgExec::XferInfo::WORKSPACE].baseFROH =
	childWorkSpaceFROH;




    rc = ProgExec::LoadAllModules(targetPID, initialXferInfo, targetXH, entry);

    if (_FAILURE(rc)) break;

    ProgExec::ConsoleGiveAccess(initialXferInfo.consoleOH, targetPID);

    ObjectHandle uprocOH;

    /* this has to work */
    (void) uProc.st._giveAccess(uprocOH, targetPID);

    //We copy this into the correct location later
    initialXferInfo.userProcessID		= targetPID;
    initialXferInfo.parentProcessID		= myPID;

    strncpy(initialXferInfo.progName, name, 64);
    initialXferInfo.progName[63] = '\0';

    initialXferInfo.typeServerObjectHandle	= tsrv;
    initialXferInfo.processObjectHandle		= uprocOH;


    // xferStart and xferStartLocal refer to the same memory, but xferStart
    // is the address at which the new process will see this stuff at
    uval xferStartLocal = workSpaceStartLocal +sizeof(initialXferInfo);
    uval xferStart = workSpaceStart + sizeof(initialXferInfo);
    uval index = 0;



    // Check if we should be debugging this executable
    // Kludge
    initialXferInfo.debugMe = GDBIO::GetDebugMe();

    ProgExec::ProcessLinuxGiveAccess(initialXferInfo.processLinuxObjectHandle,
				     uProc.st.getOH());

    // squirrel away the umask
#ifdef not_yet			// JX
    initialXferInfo.umask = FileLinux::GetUMask();
#else
    initialXferInfo.umask = 0;	// at least set it to something
#endif /* #ifdef not_yet			// JX */

    // squirrel away current working directory
    if (cwd != NULL) {
	// Started from user land
	void *p;
	p = memccpy((char *) (xferStartLocal + index), cwd, '\0',
		    ProgExec::XFERINFO_MAX_SIZE - index);
	if (p == NULL) {
	    rc = _SERROR(1468, 0, ENOMEM);
	    break;
	}

	// Update child's pointer
	initialXferInfo.cwd = (char *)(xferStart + index);
	// update and check index
	index = ALIGN_UP(uval(p) - xferStartLocal, sizeof(char *));
	if (index > ProgExec::XFERINFO_MAX_SIZE) {
	    rc = _SERROR(1469, 0, ENOMEM);
	    break;
	}

    } else {
	// running from the kernel will default to root
	initialXferInfo.cwd = NULL;
    }

    if (IOForkManager::IsInitialized()) {
	rc = IOForkManager::PreFork(targetXH);
	if (_FAILURE(rc)) break;

	rc = IOForkManager::CopyEntries((xferStartLocal + index),
					 (ProgExec::XFERINFO_MAX_SIZE- index));

	if (_FAILURE(rc)) break;

	initialXferInfo.iofmSize = _SGETUVAL(rc);
	initialXferInfo.iofmBufPtr = xferStart + index;

	index += initialXferInfo.iofmSize;
	index = ALIGN_UP(index, sizeof(char *));
	if (index > ProgExec::XFERINFO_MAX_SIZE) {
	    rc = _SERROR(1470, 0, ENOMEM);
	    break;
	}

    } else {
	initialXferInfo.iofmSize = 0;
	initialXferInfo.iofmBufPtr = 0;
    }

    ProgExec::InitMsg initMsg;

    initMsg.xferHdr = workSpaceStart;

    // The transfer area bounds are passed to the target on startup.
    initMsg.xferStart = xferStart;
    initMsg.xferSize = index;

    // Logically, the target's initial dispatcher gets all the rest of
    // workSpace, starting at the first page boundary above the xfer area.
    initMsg.dispatcherStart =
	ALIGN_UP(initMsg.xferStart + initMsg.xferSize, PAGE_SIZE);

    initialXferInfo.execStackBase = (char*)initMsg.dispatcherStart;
    // The boot stack is 4 pages right under the dispatcher.
    initMsg.dispatcherStart += ProgExec::BOOT_STACK_SIZE;

    // Dispatcher get rest of space, to the end of workspace area.
    initMsg.dispatcherSize = (workSpaceStart + ProgExec::WORKSPACE_SIZE) -
					    initMsg.dispatcherStart;




    //Set up stack region
    rc = StubRegionDefault::_CreateFixedLenExt(
	stackBottomLocal, initialXferInfo.exec.stack.memsz, 0,
	initialXferInfo.exec.localStackFR, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);

    if (_FAILURE(rc)) break;

    Obj::GiveAccessByClient(initialXferInfo.exec.localStackFR,
			    initialXferInfo.exec.stackFR, targetPID);

    initialXferInfo.region[ProgExec::XferInfo::STACK].baseFROH =
	initialXferInfo.exec.stackFR;

    stackTopLocal = stackBottomLocal +
	initialXferInfo.exec.stack.memsz;

    stackTop = initialXferInfo.exec.stack.vaddr +
	initialXferInfo.exec.stack.memsz;


    rc = ProgExec::SetupStack(stackBottomLocal, stackTopLocal, stackTop,
			      &initialXferInfo, args);

    if (_FAILURE(rc)) break;

    memcpy((void*)workSpaceStartLocal,
	   &initialXferInfo, sizeof(initialXferInfo));


    // now destroy the stack region in my address space
    // This needs to happen now, before CreateFirstDispatcher
    // Otherwise we have two mappings for the FCM, if the new process forks,
    // we die.
    DREFGOBJ(TheProcessRef)->regionDestroy(stackBottomLocal);
    stackBottomLocal = 0;

    if (workSpaceStartLocal) {
	// now destroy the xferInfo region in my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(workSpaceStartLocal);
    }


    ObjectHandle pSpecificFROH;
    // N.B. since we create this OH owned by targetPID, no
    // ReleaseAccess should be done here.  targetPID termination
    // will do it automagically.

    rc = StubFRComputation::_Create(pSpecificFROH);

    if (_FAILURE(rc)) break;


    rc = StubRegionPerProcessor::_CreateFixedAddrLenExt(
	userPSpecificRegionStart, userPSpecificRegionSize, pSpecificFROH, 0,
	(uval)(AccessMode::writeUserWriteSup), targetXH,
	RegionType::ForkCopy+RegionType::KeepOnExec);

    Obj::ReleaseAccess(pSpecificFROH);

    if (_FAILURE(rc)) break;

    rc = ProgExec::CreateFirstDispatcher(uProc.st.getOH(),
					 entry, initMsg.dispatcherStart,
					 sizeof(initMsg), (char *)(&initMsg));
    tassertMsg(_SUCCESS(rc), "CreateFirstDispatcher failed\n");

    if (_FAILURE(rc)) break;

    // for debugging purposes, we deactivate self to allow cobj gc to happen
    // it is possible it's not safe, but we assume that the test
    // infrastructure is well behaved
    if (wait) {
	Scheduler::DeactivateSelf();
	rc = uProc.st._waitForTermination();
	tassert(_SUCCESS(rc), err_printf("waitForTermination failed\n"));
	Scheduler::ActivateSelf();
    }

    // FIXME - can't destroy regions yet
    // DREF(fileRegionKernRef)->destroy();

    // FIXME - remove program checks - should just fail delete fails also
    // uProc.st._releaseAccess(); // may fail if process already gone

    rc = (SysStatusProcessID)targetPID;

} while (0);

    if (workSpaceStartLocal) {
	// now destroy the xferInfo region in my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(workSpaceStartLocal);
    }

    if (stackBottomLocal) {
	// now destroy the stack region in my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(stackBottomLocal);
    }

    if (initialXferInfo.exec.interp.localFR.valid())
	Obj::ReleaseAccess(initialXferInfo.exec.interp.localFR);

    if (initialXferInfo.exec.prog.localFR.valid())
	Obj::ReleaseAccess(initialXferInfo.exec.prog.localFR);

    if (initialXferInfo.exec.localStackFR.valid()) {
	Obj::ReleaseAccess(initialXferInfo.exec.localStackFR);
    }


    return rc;
}

void
RunProcessClose(ProgExec::BinInfo &exec)
{
    if (exec.frOH.valid()) {
	Obj::ReleaseAccess(exec.frOH);
    }
}
