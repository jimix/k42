/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kmount.C,v 1.12 2005/08/11 20:20:39 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: program to mount file systems
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <unistd.h>
#include <stdio.h>
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include <sys/ProcessSet.H>
#include <sys/systemAccess.H>
#include <cobj/sys/COSMgrObject.H>
#include <sys/MountPointMgrClient.H>

#include <fslib/fs_defines.H> // needed for fs-independent mount-flags
#include <stub/StubFileSystemNFS.H>
#include <stub/StubFileSystemK42RamFS.H>
#ifdef KFS_ENABLED
#include <stub/StubFileSystemKFS.H>
#endif // #ifdef KFS_ENABLED

static ThreadID BlockedThread = Scheduler::NullThreadID;
static uval MAX_TRY = 5;

static void
usage()
{
    printf("Usage:\n"
	   "      [-t type]            : list mounted filesystems\n"
	   "       -t type [-r] [fs-specific-options] [-d device] dir: do a mount\n"
	   "\ttype specifies desired file system. Options are kfs, nfs, ramfs"
	   "\n\tdevice specifies device being mounted, for file systems where "
	   "\n\t\tthis info makes sense (e.g. kfs)\n"
	   "\t-r: mounts file system read-only (default is rw)\n"
	   "\tfs-specific-options: list of file system specific options\n"
 	   "\t\tFor nfs:\n"
	   "\t\t\th: specifies NFS server to be mounted\n"
	   "\t\t\tp: specifies path in the server\n"
	   "\t\tFor kfs:\n"
	   "\t\t\tf: format when mounting\n"
	   "\t\tFor ramfs: no option available\n"
	   "\tdevice specifies the device being mounted\n"
	   "\tdir specifies the path where file system should be mounted\n");
}

SysStatus
listMounts(char *type)
{
    if (type[0] != '\0') {
	printf("Option -t being ignored ... will print all mounted fs\n");
    }

    printf("Look for the output of this program on the console ... "
	   "ugly, I know :-(\n");

    return DREFGOBJ(TheMountPointMgrRef)->printMtab();
}

void
startupSecondaryProcessors(char *type)
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("%s - number of processors %ld\n", type, n);
        return;
    }

    if (n > 1) {
	err_printf("%s - starting secondary processors\n", type);
    }
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passertMsg(_SUCCESS(rc), "ProgExec::CreateVP failed (0x%lx)\n", rc);
        err_printf("%s - vp %ld created\n", type, vp);
    }
}

/* static */ void
Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

#ifdef KFS_ENABLED
SysStatus
MountKFS(char *device, char *dir, uval format, uval flags)
{
    SysStatus rc;
    pid_t childLinuxPID;
    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "In MountKFS: ForkProcess() failed\n");

    if (childLinuxPID == 0) {
	// I'm the child

	// Set the cleanup delay to 100ms. so the token will circulate
	// reasonably.
	((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

	err_printf("kfs file system - starting (device %s, mount point"
		   " %s)\n", device, dir);

	/*
	 * to improve cold perfomance and thus make benchmarks easier
	 * to run, force the process set to "adapt" to supporting a
	 * large number of pids.
	 */
	DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);
	startupSecondaryProcessors("kfs");
	// FIXME: not coverable by default
	// FIXME: we want to pass flags
	rc = StubFileSystemKFS::_Mkfs(device, strlen(device),
				      dir, strlen(dir), format, flags);
	if (_FAILURE(rc)) {
	    err_printf("Failure on kfs startup  (device %s, path %s): rc is"
		       " (%ld, %ld, %ld)\n", device, dir, _SERRCD(rc),
		       _SCLSCD(rc), _SGENCD(rc));
	}
	Block();
    }

    // this is for the parent: wait for file system initialization to finish

    uval cont = 0;
    do {
#ifdef MAMBO
	Scheduler::DelayMicrosecs(10000);
#else
 	Scheduler::DelaySecs(1);
#endif

	rc = StubFileSystemKFS::_TestAlive(dir, strlen(dir));
	if (_FAILURE(rc)) {
	    if (!cont && _SGENCD(rc) == ENOENT) {
		printf("waiting for kfs initialization\n");
	    } else {
		//err_printf("\trc is (%ld,%ld, %ld) \n", argv[0], _SERRCD(rc),
		//	       _SCLSCD(rc), _SGENCD(rc));
	    }
	}
	cont++;
    } while (_FAILURE(rc) && cont < MAX_TRY);

    if (_FAILURE(rc)) {
	printf("Mounting of KFS for device %s, path %s failed with rc=0x%lx\n",
	       device, dir, rc);
    }

    return rc;
}
#endif // #ifdef KFS_ENABLED

static SysStatus
MountNFS(char *host, char *rpath, char *mpath)
{
    SysStatus rc;
    pid_t childLinuxPID;
    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "In MountNFS: ForkProcess() failed\n");

    if (childLinuxPID == 0) {
	// I'm the child

	// Set the cleanup delay to 100ms. so the token will circulate
	// reasonably.
	((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

	err_printf("fs file system - starting (host %s, rpath %s, mpath %s)"
		   "\n", host, rpath, mpath);

	/*
	 * to improve cold perfomance and thus make benchmarks easier
	 * to run, force the process set to "adapt" to supporting a
	 * large number of pids.
	 */
	DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);
	startupSecondaryProcessors("kfs");
	// FIXME: pass uids properly
	// FIXME: not coverable by default
	// FIXME: we want to pass flags
	rc = StubFileSystemNFS::_Mkfs(host, strlen(host), rpath, strlen(rpath),
				      mpath, strlen(mpath), -1, -1, 0);
	if (_FAILURE(rc)) {
	    err_printf("Failure on nfs startup  (host %s, rpath %s, mpath %s):"
		       " rc is (%ld, %ld, %ld)\n", host, rpath, mpath,
		       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	}
	Block();
    }

    // this is for the parent: wait for file system initialization to finish

    uval cont = 0;
    do {
#ifdef MAMBO
	Scheduler::DelayMicrosecs(10000);
#else
 	Scheduler::DelaySecs(1);
#endif

	rc = StubFileSystemNFS::_TestAlive(mpath, strlen(mpath));
	if (_FAILURE(rc)) {
	    if (!cont && _SGENCD(rc) == ENOENT) {
		err_printf("waiting for nfs initialization\n");
	    } else {
		//err_printf("\trc is (%ld,%ld, %ld) \n", argv[0], _SERRCD(rc),
		//	       _SCLSCD(rc), _SGENCD(rc));
	    }
	}
	cont++;
    } while (_FAILURE(rc) && cont < MAX_TRY);

    if (_FAILURE(rc)) {
	printf("Mounting of NFS for server %s, remote path %s, mount path %s"
	       " failed with rc=0x%lx\n", host, rpath, mpath, rc);
    }

    return rc;
}

static SysStatus
MountRamfs(char *dir)
{
    SysStatus rc;

    pid_t childLinuxPID;
    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "In MountKFS: ForkProcess() failed\n");

    if (childLinuxPID == 0) {
	// I'm the child

	// Set the cleanup delay to 100ms. so the token will circulate
	// reasonably.
	((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

	err_printf("ramfs file system - starting (mount point %s)\n", dir);

	/*
	 * to improve cold perfomance and thus make benchmarks easier
	 * to run, force the process set to "adapt" to supporting a
	 * large number of pids.
	 */
	DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);
	startupSecondaryProcessors("ramfs");
	// FIXME: not coverable by default
	// FIXME: we want to pass flags
	rc = StubFileSystemK42RamFS::_Mkfs(dir, strlen(dir));
	if (_FAILURE(rc)) {
	    err_printf("Failure on ramfs startup  (path %s): rc is"
		       " (%ld, %ld, %ld)\n", dir, _SERRCD(rc),
		       _SCLSCD(rc), _SGENCD(rc));
	}
	Block();
    }

    // this is for the parent: wait for file system initialization to finish

    uval cont = 0;
    do {
#ifdef MAMBO
	Scheduler::DelayMicrosecs(10000);
#else
 	Scheduler::DelaySecs(1);
#endif

	rc = StubFileSystemK42RamFS::_TestAlive(dir, strlen(dir));
	if (_FAILURE(rc)) {
	    if (!cont && _SGENCD(rc) == ENOENT) {
		err_printf("waiting for ramfs initialization\n");
	    } else {
		//err_printf("\trc is (%ld,%ld, %ld) \n", argv[0], _SERRCD(rc),
		//	       _SCLSCD(rc), _SGENCD(rc));
	    }
	}
	cont++;
    } while (_FAILURE(rc) && cont < MAX_TRY);

    if (_FAILURE(rc)) {
	printf("Mounting of ramfs (K42Ramfs) forpath %s failed with "
	       "rc=0x%lx\n", dir, rc);
    }
    return rc;
}

int
main(int argc, char *argv[])
{
    NativeProcess();

    int c;
    extern int optind;
    extern char *optarg;
    const char *optlet = "t:d:h:p:rf";
    char type[32] = {0,};
    char device[128] = {0,};
    char host[128] = {0,};   // NFS specific
    char rpath[256] = {0,};  // NFS specific
    uval format = 0; // KFS specific
    uval readOnly = 0;
    uval isMountCommand = 0; // indicates arguments for mount've been provided

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 't':
	    passertMsg(strlen(optarg) < sizeof(type), "no space");
	    strncpy(type, optarg, strlen(optarg));
	    break;
	default:
	    isMountCommand = 1;
	    switch (c) {
	    case 'd':
		passertMsg(strlen(optarg) < sizeof(device), "no space");
		strncpy(device, optarg, strlen(optarg));
	    break;
	    case 'r':
		readOnly = 1;
		break;
	    case 'f': // option is KFS-specific, it'll be ignored otherwise
		format = 1;
		break;
	    case 'h': // NFS-specific, ignored otherwise
		passertMsg(strlen(optarg) < sizeof(host), "no space");
		strncpy(host, optarg, strlen(optarg));
		break;
	    case 'p': // NFS-specific, ignored otherwise
		passertMsg(strlen(optarg) < sizeof(rpath), "no space");
		strncpy(rpath, optarg, strlen(optarg));
		break;
	    case '?':
	    default:
		usage();
		return 1;
	    }
	}
    }

    if (argc <= optind) { // no more arguments
	if (isMountCommand) {
	    printf("%s: missing arguments\n", argv[0]);
	    usage();
	    return 1;
	} else { // user wants list of current mounts
	    SysStatus rc = listMounts(type);
	    int ret = (_SUCCESS(rc) ? 0 : 1);
	    return ret;
	}
    }

    if (type[0] == 0) {
	printf("%s: file system type not specified\n", argv[0]);
	usage();
	return 1;
    }

    if (format && strncmp(type, "kfs", 3) != 0) {
	printf("%s: Option -f is valid for kfs only.\n", argv[0]);
	usage();
	return 1;
    }

    char *dir = argv[optind];

    SysStatus rc = -1;
    if (strncmp(type, "kfs", 3) == 0) {
	if (device[0] == 0) {
	    printf("%s: missing device for KFS\n", argv[0]);
	    usage();
	    return 1;
	}
#ifdef KFS_ENABLED
	uval flags = 0;
	if (readOnly) {
	    flags = MS_RDONLY;
	}
	rc = MountKFS(device, dir, format, flags);
#else
	printf("%s: KFS is not enabled (see Make.paths, FILESYS should "
	       "specify KFS\n", argv[0]);
	return 1;
#endif // #ifdef KFS_ENABLED
    } else if (strncmp(type, "nfs", 3) == 0) {
	rc = MountNFS(host, rpath, dir);
    } else if (strncmp(type, "ramfs", 5) == 0) {
	rc = MountRamfs(dir);
    } else {
	printf("%s: file system type %s unknown. Supported types are kfs, "
	       "nfs, ramfs\n", argv[0], type);
    }

    if (_SUCCESS(rc)) {
	return 0;
    } else {
	return 1;
    }
}
