/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysctl.C,v 1.31 2005/07/21 19:29:01 dilma Exp $
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FS-based tracemask control facility
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "FileTemplate.H"

//#include "ProcServer.H"
//#include <stub/StubProcServer.H>

#include <stub/StubWire.H>
#include <sys/MountPointMgrClient.H>
#include <sys/systemAccess.H>
#include "TextFile.H"

extern FileInfoVirtFSDirBase* getDir();

SysStatus
bindFS(char* from, char* to)
{
    SysStatus rc;

    PathNameDynamic<AllocGlobal> *oldPath, *newPath;
    uval oldlen, newlen, maxpthlen;

    rc = FileLinux::GetAbsPath(from, oldPath, oldlen, maxpthlen);
    if (_FAILURE(rc)) {
	err_printf("GetAbsPath for %s failed\n", from);
	return rc;
    }
    rc = FileLinux::GetAbsPath(to, newPath, newlen, maxpthlen);
    if (_FAILURE(rc)) {
	err_printf("GetAbsPath for %s failed\n", to);
	return rc;
    }

    return DREFGOBJ(TheMountPointMgrRef)->bind(oldPath, oldlen,
					       newPath, newlen, 0);
}

class TraceMaskFile: public VirtFileUval {
public:
    TraceMaskFile(char* fileName):
	VirtFileUval(fileName,"TraceMask: ") {};
    virtual SysStatusUval getUval() {
	uval mask;
	DREFGOBJ(TheSystemMiscRef)->traceGetMask(mask);
	return mask;
    }
    virtual SysStatus setUval(uval val) {
	SysStatus rc = DREFGOBJ(TheSystemMiscRef)->traceSetMaskAllProcs(val);
	return rc;
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __in uval userData) {
	SysStatus rc;
	/* reset */
	if (length == 6 && 0 == memcmp(buf, "reset\n", 6)) {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceResetAllProcs();
	    if (_SUCCESS(rc)) {
		return length;
	    } else return rc;
	} else {
	    return VirtFileUval::_write(buf, length, userData);
	}
    }

    DEFINE_GLOBAL_NEW(TraceMaskFile);
};

class HWPerfToggle: public VirtFileUval {
public:
    HWPerfToggle(char* fileName):
	VirtFileUval(fileName,"HWPerfToggle (not available): ") {};
    virtual SysStatusUval getUval() {
	return 0;
    }
    virtual SysStatus setUval(uval val) {
        // REZA: FIXME: The hw perf programming model has changed
	// if (val) {
	//     return DREFGOBJ(TheSystemMiscRef)->hwPerfEnableAllProcs();
	// }
	// return DREFGOBJ(TheSystemMiscRef)->hwPerfDisableAllProcs();
	return 0;
    }
    DEFINE_GLOBAL_NEW(HWPerfToggle);
};

class HWPerfPeriod: public VirtFileUval {
public:
    HWPerfPeriod(char* fileName):
	VirtFileUval(fileName,"HWPerf Sample Period (not available): ") {};
    virtual SysStatusUval getUval() {
	return 0;
    }
    virtual SysStatus setUval(uval val) {
	if (val) {
	    if (val<10000) {
		err_printf("HW perf period should be > 10000\n");
		val=10000;
	    }
	    // return DREFGOBJ(TheSystemMiscRef)->hwPerfStartAllProcs(val);
	}
	// return DREFGOBJ(TheSystemMiscRef)->hwPerfStopAllProcs();
	return 0;
    }
    DEFINE_GLOBAL_NEW(HWPerfPeriod);
};

class NFSRevalidate: public VirtFileUval {
public:
    NFSRevalidate(char* fileName):
	VirtFileUval(fileName,"NFS Revalidation Toggle (not available): ") {};
    virtual SysStatusUval getUval() {
	return 0;
    }
    virtual SysStatus setUval(uval v) {
	uval nval = (v ? 1 : 0);
	return DREFGOBJ(TheSystemMiscRef)->setControlFlagsBit(
	    KernelInfo::NFS_REVALIDATION_OFF, nval);
    }
    DEFINE_GLOBAL_NEW(NFSRevalidate);
};

class WireDaemon: public VirtFileUval {
public:
    int val;
    WireDaemon(char* fileName):
	VirtFileUval(fileName,"HWPerf Sample Period (not available): ") {
	val=1;
    };
    virtual SysStatusUval getUval() {
	return val;
    }
    virtual SysStatus setUval(uval val) {
	if (val) {
	    return StubWire::RestartDaemon();
	}
	return StubWire::SuspendDaemon();
    }
    DEFINE_GLOBAL_NEW(WireDaemon);
};

class InterceptNFS: public VirtFileUval {
public:
    int val;
    InterceptNFS(char* fileName):
	VirtFileUval(fileName,"NFS Interception toggle") {
	val=0;
    };
    virtual SysStatusUval getUval() {
	return val;
    }
    virtual SysStatus setUval(uval v) {
	val = (v ? 1 : 0);
	return DREFGOBJ(TheSystemMiscRef)->setControlFlagsBit(
	    KernelInfo::NFS_INTERCEPTION, val);
    }
    DEFINE_GLOBAL_NEW(InterceptNFS);
};
static void
startupSecondaryProcessors(char *prog)
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if ( n <= 1) {
        err_printf("%s - number of processors %ld\n", prog, n);
        return ;
    }

    err_printf("%s - starting %ld secondary processors\n", prog, n-1);
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("%s - vp %ld created\n", prog, vp);
    }
}

SysStatus
addFile(VirtNamedFile* vf, FileInfoVirtFSDir* sysFS)
{
    SysStatus rc;

    rc = sysFS->add(vf->name, strlen(vf->name), vf);
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == EEXIST) {
	    printf("sysctl: File %s already exists\n",  vf->name);
	} else if (_SGENCD(rc) == ENOTDIR) {
	    printf("sysctl: a component used as a directory in pathname %s "
		   "is not, in fact, a directory\n",  vf->name);
	} else if (_SGENCD(rc) == ENOENT) {
	    printf("sysctl: a directory component in %s does not exist\n",
		    vf->name);
	} else {
	    printf("sysctl: FileLinuxVirtFile::CreateFile for %s failed: "
		   "(%ld, %ld, %ld)\n",  vf->name, _SERRCD(rc),
		   _SCLSCD(rc), _SGENCD(rc));
	}
    }
    return rc;
}


typedef void (*init_fn_t)(FileInfoVirtFSDir* ref);
#define DECLARE_INIT(fn) extern void fn(FileInfoVirtFSDir* ref);
INIT_DECLS;
 /*
  * INIT_FUNCS list generated in Makefile.  Functions to be called on
  * initialization
  */
init_fn_t init_fns[] = {INIT_FUNCS};

int
main(int argc, char *argv[])
{
    NativeProcess();

    if (argc == 1) {
	close(0);
	close(1);
	close(2);
    }

    setpgid(0,0);
    pid_t pid = fork();
    if (pid) {
	struct stat buf;
	while (stat("/ksys",&buf)!=0) usleep(10000);
	exit(0);
    }

    startupSecondaryProcessors(argv[0]);
//    mkdir("/virtfs/sys",0777);
//    mkdir("/virtfs/sys/hwperf",0777);
//    bindFS("/virtfs/sys", "/sys");

//    ProcServer::ClassInit(0);
//    bindFS("/virtfs/proc", "/proc");
    FileInfoVirtFSDirStatic root;
    root.init((mode_t)0755);
    root.mountToNameSpace("/ksys");

    addFile(new TraceMaskFile("traceMask"), &root);
    addFile(new HWPerfToggle("toggle"), &root);
    addFile(new HWPerfPeriod("period"), &root);
    addFile(new NFSRevalidate("nfsRevalidationOff"), &root);
    addFile(new WireDaemon("wireDaemon"), &root);
    addFile(new InterceptNFS("interceptNFS"), &root);
    addFile(new VirtFileText("hostname"), &root);
    uval i = 0;
    while (init_fns[i]) {
	(*init_fns[i])(&root);
	++i;
    }


    // deactivate and block this child's thread forever
    Scheduler::DeactivateSelf();
    while (1) {
	Scheduler::Block();
    }
    return 0;
}
