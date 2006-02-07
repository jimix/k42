/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: baseServer.C,v 1.143 2005/06/28 19:47:48 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <stub/StubBaseServers.H>
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include "BaseServers.H"
#include "PrivilegedServiceWrapper.H"
#include <sys/SystemMiscWrapper.H>
#include <sys/KernelInfo.H>
#include <stub/StubFSFRSwap.H>
#include <defines/paging.H>
#include <stub/StubKBootParms.H>
#include <sys/ProcessLinuxClient.H>
#include <io/FileLinux.H>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <alloc/PageAllocatorUser.H>
#include "fileSystemServices.H"
#include <sys/InitStep.H>
#include <sys/Initialization.H>
#include <sys/systemAccess.H>
#include "FileSystemDev.H"
#include "MountPointMgrImp.H"
#include "ResMgr.H"
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
extern void* _k42start;
volatile void *pull_in_k42start= _k42start;

void
StartupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if ( n<=1) {
        err_printf("base Servers - number of processors %ld\n", n);
        return ;
    }

    err_printf("base Servers - starting %ld secondary processors\n",n-1);
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("base Servers - vp %ld created\n", vp);
    }
}

#include "StreamServerPipe.H"
#include "StreamServerSocket.H"
#include "Login.H"
#include <sys/ProcessLinux.H>

// Can't include LinuxPTY --- headers blow up.
// Define this to access ClassInit instead.
// #include "LinuxPTY.H"
extern void LinuxPTYServer_ClassInit();

#include <unistd.h>
//#include "AdmCtrl.H"

#include "loadImage.H"

#include <cobj/sys/COSMgrObject.H>
#include <sys/ProcessSet.H>

#include <emu/FD.H>

#include <io/FileLinuxStreamTTY.H>

#include "SysVSemaphores.H"
#include "SysVSharedMem.H"
#include "SysVMessages.H"

SysStatus
setStdFD() {
    FileLinuxRef userConRef;
    FileLinuxRef tmpFileRef;
    SysStatus rc;

    rc = FileLinuxStreamTTY::Create(userConRef,
				    ProgExec::ExecXferInfo->consoleOH, O_RDWR);
    _IF_FAILURE_RET(rc);

    rc = DREF(userConRef)->dup(tmpFileRef);
    if (_SUCCESS(rc)) {
	DREF(tmpFileRef)->setFlags(O_RDONLY);
	_FD::ReplaceFD(tmpFileRef, 0);
    }

    rc = DREF(userConRef)->dup(tmpFileRef);
    if (_SUCCESS(rc)) {
	DREF(tmpFileRef)->setFlags(O_WRONLY);
	_FD::ReplaceFD(tmpFileRef, 1);
    }

    rc = DREF(userConRef)->dup(tmpFileRef);
    if (_SUCCESS(rc)) {
	DREF(tmpFileRef)->setFlags(O_WRONLY);
	_FD::ReplaceFD(tmpFileRef, 2);
    }

    DREF(userConRef)->destroy();

    return 0;
}

#include <io/FileLinux.H>

INIT_OBJECT(SwapFileInit, INIT_SWAPFILE);

// runs as a clone() thread, so must exit() at end
void
RunInitSwapFile(uval rcPtr) 
{
    SysStatus rc;
    ObjectHandle fileFROH;
    FileLinuxRef flr;

    char *nfsSwapFile = "/tmp/.SWAP";
    char *kfsSwapFile = "/kkfs/.SWAP";

    // let's try /kkfs/.SWAP
    rc = FileLinux::Create(flr, kfsSwapFile, O_RDWR|O_CREAT, 0644);
    if (_SUCCESS(rc)) {
	err_printf("initializing swap file system to %s\n", kfsSwapFile);
    } else { // let's do /tmp/.SWAP (which is probably on nfs)
	rc = FileLinux::Create(flr, nfsSwapFile, O_RDWR|O_CREAT, 0644);
	passertMsg(_SUCCESS(rc), "open of swap file at %s failed\n",
		   nfsSwapFile);
	err_printf("initializing swap file system to %s\n", nfsSwapFile);
    }

    rc = DREF(flr)->getFROH(fileFROH, FileLinux::DEFAULT);
    tassert(_SUCCESS(rc), err_printf("getFROH of swap file failed\n"));

    // okay now call the kernel to initialize swap
    StubFSFRSwap::_SetFR(fileFROH);
    _exit(0);
}


//
// Run "fn" in a new clone-thread.  Wait for completion.
//
sval
runCloneThread(Scheduler::ThreadFunction fn, uval arg)
{
    SysStatus rc;
    pid_t pid;

    rc = DREFGOBJ(TheProcessLinuxRef)->cloneNative(pid, fn, arg);
    tassertMsg(_SUCCESS(rc), "cloneNative failure: %lx\n",rc);

    // FIXME: waitpid() implementation requires WNOHANG, but even if
    // fixed, not known how it will react to a non-clone thread
    // waiting for a clone() thread to exit.
    sval status;
    do {
	pid = -1;
	rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(pid, status,
						   WNOHANG|__WCLONE);
	Scheduler::Yield();
    } while (_FAILURE(rc) || pid == 0);

    return status;
}

/* virtual */ SysStatus
SwapFileInit::action()
{
    err_printf("Starting swap file registration\n");
#if (SWAPFS == FRSWAPFS) && defined(NFS_PROVIDES_SWAPFR)
    runCloneThread(RunInitSwapFile, 0);
#endif
    return 0;
}

INIT_OBJECT_PTR(SocketInit, "Unix domain socket services", INIT_SOCKET,
		StreamServerSocket::ClassInit);

INIT_OBJECT_PTR(PipeInit, "pipe services", INIT_PIPE,
		StreamServerPipe::ClassInit);

INIT_OBJECT_PTR(PTYInit, "pty services", INIT_PTY,
		LinuxPTYServer_ClassInit);

INIT_OBJECT_PTR(DevFSInit, "/dev file system", INIT_DEVFS,
		FileSystemDev::ClassInit);

INIT_OBJECT_PTR(SysVSemInit, "SYS V Semaphores", INIT_SYSVSEM,
		SysVSemaphores::ClassInit);

INIT_OBJECT_PTR(SysVMemInit, "SYS V Shared Memory", INIT_SYSVMEM,
		SysVSharedMem::ClassInit);

INIT_OBJECT_PTR(SysVMsgInit, "SYS V Messages", INIT_SYSVMSG,
		SysVMessages::ClassInit);

INIT_OBJECT_PTR(MntPtMgrInit, "Mount-Point Manager", INIT_MNTPTMGR,
		MountPointMgrImp::ClassInit);

INIT_OBJECT_PTR(FSInit, "file systems", INIT_FSSERVICE,
		StartFileSystemServices);


static void
StartServices()
{
    new MntPtMgrInit();
    new PipeInit();
    new SocketInit();
    new DevFSInit();
    new PTYInit();
    new SysVMemInit();
    new SysVSemInit();
    new SysVMsgInit();
    //BaseServers::ClassInit();
    new FSInit();

    SysStatus rc;
    err_printf("baseServers launching init.\n");
    rc = DREFGOBJ(TheSystemMiscRef)->
	launchProgram("baseServers", "-init", "", /*wait*/ 0);
    tassertMsg(_SUCCESS(rc), "Could not launch init.\n");
}

void
RunInitScript()
{
    SysStatus rc;
    char *argv[] = {"sysinit", NULL};
    char *envp[] = {NULL};
    char buf[64]={0,};

    rc = DREFGOBJ(TheProcessLinuxRef)->becomeLinuxProcess();
    tassertMsg(_SUCCESS(rc), "Could not become linux process: %lx\n",rc);

    rc = StubKBootParms::_GetParameterValue("K42_INITSCR", buf, 64);

    if (_FAILURE(rc) || strlen(buf) == 0) {
        err_printf("%s: nothing to do since K42_INITSCR = '%s'\n",
                   __FUNCTION__, buf);
	return;
    }

    if (buf[0]=='/') {
	argv[0]=buf;
    }

    //Set up stdout and stderr.  No stdin.
    Login::ConsoleProcess(argv, envp);
}

void
RLoginDaemon()
{
    SysStatus rc;
    err_printf("Login daemon starting\n");
    rc = DREFGOBJ(TheProcessLinuxRef)->becomeLinuxProcess();
    Login::ClassInit();
    tassertMsg(0, "RLoginDaemon:  Login returned!!\n");
}

INIT_OBJECT(InitScriptInit, INIT_SCRIPT);

/* virtual */ SysStatus
InitScriptInit::action()
{
    SysStatus rc;

    err_printf("Running system initialization script.\n");

    rc = DREFGOBJ(TheSystemMiscRef)->
	launchProgram("baseServers", "-script", "",/*wait*/ 1);
    tassertMsg(_SUCCESS(rc), "Could not launch script.\n");
    return rc;
}


//
// Use "releaseMain" as a flag to stop progress of main thread on it's
// way to becoming init process.
//
INIT_OBJECT(ReaperInit, INIT_REAPER);


volatile ThreadID releaseMain = Scheduler::NullThreadID;
/* virtual */ SysStatus
ReaperInit::action()
{
    err_printf("Starting process reaper\n");
    ThreadID id = releaseMain;
    releaseMain = Scheduler::GetCurThread();
    Scheduler::Unblock(id);
    while (releaseMain != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    return 0;
}


INIT_OBJECT(RLoginInit, INIT_RLOGIN);

/* virtual */ SysStatus
RLoginInit::action()
{
    SysStatus rc = 0;
    err_printf("Starting rlogin daemon\n");

    rc = DREFGOBJ(TheSystemMiscRef)->
	launchProgram("baseServers", "-rlogin", "",/*wait*/ 0);
    tassertMsg(_SUCCESS(rc), "Could not launch rlogin.\n");
    return rc;
}

INIT_OBJECT(KFSInit, INIT_KFS);

/* virtual */ SysStatus
KFSInit::action()
{
    SysStatus rc = 0;

#ifdef KFS_ENABLED
    runCloneThread(RunInitKFS, (uval) &rc);
#endif // #ifdef KFS_ENABLED
    return rc;
}


INIT_OBJECT(NFSInit, INIT_NFS);

/*virtual*/ SysStatus
NFSInit::action()
{
    SysStatus rc;
    runCloneThread(StartNFS, (uval) &rc);
    return rc;
}

INIT_OBJECT(Ext2Init, INIT_EXT2);

/* virtual */ SysStatus
Ext2Init::action()
{
    SysStatus rc = 0;

#ifdef EXT2_ENABLED
    runCloneThread(StartExt2, (uval)&rc);
#endif // #ifdef EXT2_ENABLED
    return rc;
}

void
PrepReaper()
{
    SysStatus rc;
    sval32 status;
    rc = DREFGOBJ(TheProcessLinuxRef)->becomeInit();
    tassertMsg(_SUCCESS(rc), "InitProgram: could not become init!!\n");

    /*
     * This new instance of baseServers needs a VP on every processor.
     */
    StartupSecondaryProcessors();

    new InitScriptInit();
    new NFSInit();
    new SwapFileInit();
    new KFSInit();
    new Ext2Init();
    new RLoginInit();
    new ReaperInit();


    releaseMain = Scheduler::GetCurThread();
    // Note interactions with "StartReaper"
    while (releaseMain== Scheduler::GetCurThread()) {
	Scheduler::Block();
    }
    releaseMain = Scheduler::GetCurThread();

    // collect orphans
    for (;;) {
	pid_t pid;
	pid = waitpid(-1, &status, 0);

	// This error condition occurs if the first child (rlogin)
	// has not yet become a linux process by the time we get here
	tassertWrn(pid > 0 || errno==ECHILD,
		   "waitpid() failed: %s.\n", strerror(errno));
	if (pid<=0 && errno==ECHILD) {
	    Scheduler::YieldProcessor();
	} else if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("InitProgram: reaped pid: 0x%lx\n", (sval)pid);
	}
    }
}





static void
DoLoadImage(int argc, char *argv[])
{
    SysStatus rc;

    rc = DREFGOBJ(TheProcessLinuxRef)->becomeLinuxProcess();
    tassertMsg(_SUCCESS(rc),
	       "DoLoadImage: could not become a linux process!!\n");
    const char *image = (argc > 2) ? argv[2] : "/knfs/boot/boot_image";
    (void) loadImage(argv[0], image);
}

static void
PrintLinuxStatus()
{
    (void) StubProcessLinuxServer::_PrintStatus();
}

int
main(int argc, char *argv[], char *envp[])
{
    NativeProcess();

    SysStatus rc;

    // This is a special-case app that links in libk42sys.so statically,
    // so we mark ourselves as using K42 interfaces; this is not a pure
    // Linux app.
    extern uval useLongExec;
    useLongExec = 1;

    rc = setStdFD();
    tassertMsg(_SUCCESS(rc),
	       "baseServers: Could not set up std fd's: %lx\n",rc);

    if (strcmp(argv[0], "baseServers") != 0) {
	// We were invoked just as a proxy to load another process.
	rc = DREFGOBJ(TheProcessLinuxRef)->becomeLinuxProcess();
	tassertMsg(_SUCCESS(rc),
		   "baseServers(%s): could not become a linux process!!\n",
		   argv[0]);
	Login::ConsoleProcess(argv, envp);
	return 0;
    }

    if (argc > 1) {
	if (strcmp(argv[1], "-script") == 0) {
	    RunInitScript();
	} else if (strcmp(argv[1], "-init") == 0) {
	    PrepReaper();
	} else if (strcmp(argv[1], "-rlogin") == 0) {
	    RLoginDaemon();
	} else if (strcmp(argv[1], "-d") == 0) {
	    DoLoadImage(argc, argv);
	} else if (strcmp(argv[1], "-ps") == 0) {
 	    PrintLinuxStatus();
	} else {
	    err_printf("%s: unknown parameter \"%s\".\n", argv[0], argv[1]);
	}
	return 0;
    }

    /*
     * If we get here, we were invoked as "baseServers" without arguments,
     * which means we're supposed to be the initial server launched from the
     * kernel.  We should have acquired access to kernel privileged services
     * in ProgExec::InitCustomizationHook() (in crtServer.C).  If we didn't, it
     * means that someone has relaunched us incorrectly, and we bail out.
     */
    if (PrivilegedServiceWrapper::ThePrivilegedServiceRef() == NULL) {
	err_printf("Bogus attempt to relaunch baseServers.\n");
	return -1;
    }

    //
    // Tuning for benchmarks
    //
    // Set the cleanup delay to 100ms. so the token will circulate
    // reasonably.
    ((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

    // FIXME: Spin longer for all BLocks.
    extern uval FairBLockSpinCount, BitBLockSpinCount;
    FairBLockSpinCount = 1000000;
    BitBLockSpinCount = 1000000;

    /*
     * to improve cold perfomance and thus make benchmarks easier
     * to run, force the process set to "adapt" to supporting a
     * large number of pids.
     */
    DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);

    StartupSecondaryProcessors();
    StartServices();

    // deactivate and block this main thread forever
    Scheduler::DeactivateSelf();
    while (1) {
	Scheduler::Block();
    }
    // NOTREACHED
    tassertSilent(0, err_printf("baseServers: Should never get here\n"));
}
