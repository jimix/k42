/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fileSystemServices.C,v 1.80 2005/08/16 21:53:11 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "fileSystemServices.H"
#include <misc/baseStdio.H>
#include <stub/StubMountPointMgr.H>
#include <stub/StubKBootParms.H>
#include <misc/DiskMountInfoList.H>
#include <fslib/fs_defines.H> // needed for fs-independent mount-flags
#include "FileSystemNFS.H"
#include "FileSystemK42RamFS.H"
#include "FileSystemDev.H"
#include "MountPointMgrImp.H"

#ifdef KFS_ENABLED
#include <misc/ListSimple.H>
#include <io/FileLinux.H>
#include <FileSystemKFS.H>
#include <usr/ProgExec.H>
#include <emu/FD.H>
// FIXME: THIS HAS TO BE DONE DIFFERENTLY!! We need SIM_MAMBO, hopefully
//        someone will make it available in KernelInfo so we don't have
//        to include machine-dependent code
#include "../../kernel/bilge/arch/powerpc/BootInfo.H"

#endif

#ifdef EXT2_ENABLED
#include "FileSystemExt2.H"
#endif

#include <unistd.h>
#include <sys/ProcessLinuxClient.H>

typedef enum {NFS, KFS, EXT2} RootType;

struct nfsMountData {
#define ROOT_ONLY	      0x1
#define NOPRUNING	      0x2
#define READ_ONLY	      0x4
#define DO_BINDINGS_ROOT_ONLY 0x8
#define NFS_INVALID	      ~0ULL
    char* host;
    char* path;
    char* mntPoint;
    uval flags;
    struct {
	char* from;
	char* to;
    }bindings[10];
};
#define NOBINDINGS  {{NULL,NULL}}

char k42_pkghost[64] = {0,};
char k42_pkgroot[64] = {0,};

struct nfsMountData nfsMounts[] = {

    { k42_pkghost, k42_pkgroot , "/", ROOT_ONLY, NOBINDINGS},
    { k42_pkghost, k42_pkgroot , "/nfs", NOPRUNING, NOBINDINGS},
    { NULL, NULL, "/knfs", NOPRUNING | DO_BINDINGS_ROOT_ONLY,
      {
	  {"/knfs/home","/home"},
	  {"/knfs/klib","/klib"},
	  {"/knfs/kbin","/kbin"},
	  {"/knfs/etc","/etc"},
	  {"/knfs/tests", "/tests"},
	  {"/knfs/tmp", "/tmp"},
	  {"/knfs/root", "/root"},
	  {"/knfs/usr/tmp", "/usr/tmp"},
	  {NULL, NULL}
      }
    },
//    { "9.2.224.107", "/a/kitch0/homes/kitch0/k42/htdocs",
//      "/usr/local/apache2/htdocs", 0, NOBINDINGS},
#ifdef MOUNT_NFS_OTHER_ROOT
    /* By creating another NFS instance for kitchroot we have a way
     * of faking remote changes so that we can test our NFS caching scheme
     */
    { NULL, NULL, "/nfs-otherRoot", 0, NOBINDINGS},
#endif
    { NULL, NULL, NULL, NFS_INVALID, {{NULL,NULL},} },
};

#ifdef KFS_ENABLED
// FIXME: unify this data structure with nfsMounts
struct kfsMountData {
#define ROOT_ONLY	      0x1
#define NOPRUNING	      0x2
#define DO_BINDINGS_ROOT_ONLY 0x8
#define FORMAT                0x1
    char *name;
    uval flags;       // these guide if/how we invoke FileSystemKFS::Create
    uval fsFlags;     /* these are passed directly to FileSystemKFS::Create
		       * e.g. MS_RDONLY */
    uval format;
    struct {
	char* from;
	char* to;
    } bindings[10];
};

struct kfsMountData kfsMounts[] = {
    /* RM: mount only if KFS is root file system, and mount as read only with
     *     no bindings */
    {"RM", ROOT_ONLY, MS_RDONLY, 0, NOBINDINGS},
    // myRM: similar to the way RM works, but we allow mounting in this
    //       configuration even if not mounting as root
    {"myRM", NOPRUNING, MS_RDONLY, 0, NOBINDINGS},
    // kroot: mount options designed for mouting our kitchroot, i.e.,
    //          with list of bindings done only if KFS is root.
    {"kroot", NOPRUNING |  DO_BINDINGS_ROOT_ONLY, 0, 0,
     {
	 {"/kkfs/home","/home"},
	 {"/kkfs/klib","/klib"},
	 {"/kkfs/kbin","/kbin"},
	 {"/kkfs/etc","/etc"},
	 {"/kkfs/tests", "/tests"},
	 {"/kkfs/tmp", "/tmp"},
	 {"/kkfs/root", "/root"},
	 {"/kkfs/usr/tmp", "/usr/tmp"},
	 {NULL, NULL}
     }
    },
    // krootF: similar to kroot spec, but the file system will be formatted
    //         on mount. This is useful to experiment with KFS on victms where
    //         you can't rely on content of disk.
    {"krootF", NOPRUNING |  DO_BINDINGS_ROOT_ONLY, 0, FORMAT,
     {
	 {"/kkfs/home","/home"},
	 {"/kkfs/klib","/klib"},
	 {"/kkfs/kbin","/kbin"},
	 {"/kkfs/etc","/etc"},
	 {"/kkfs/tests", "/tests"},
	 {"/kkfs/tmp", "/tmp"},
	 {"/knfs/root", "/root"},
	 {"/kkfs/usr/tmp", "/usr/tmp"},
	 {NULL, NULL}
     }
    },
    {"simplekfsFormat", NOPRUNING, 0, FORMAT, NOBINDINGS},
    {"simplekfs", NOPRUNING, 0, 0, NOBINDINGS},
    {NULL, 0, 0, 0, NOBINDINGS},
};

#endif //#ifdef KFS_ENABLED

RootType getRoot()
{
    RootType root = NFS;

    char buf[64] = {0,};
    SysStatus rc = StubKBootParms::_GetParameterValue("K42_ROOT_FS", buf, 64);
    if (_FAILURE(rc) || buf[0] == 0) {
	err_printf("K42_ROOT_FS not specified. Using NFS as root.\n ");
	/* Some time in the very near future we'll want to have KFS as the
	 * the default in the simulator, but not now when migrating to
	 * have KFS_ENABLED the default */
    } else {
	if (strcmp(buf, "NFS") == 0) {
	    err_printf("Root file system is NFS\n");
	} else if (strcmp(buf, "KFS") == 0) {
#ifdef KFS_ENABLED
	    err_printf("Root file system is KFS\n");
	    root = KFS;
#else
	    err_printf("You have set K42_ROOT_FS = KFS, but not compiled "
		       "KFS support, so I will use NFS as root filesystem.\n");
#endif //#ifdef KFS_ENABLED

	} else if (strcmp(buf, "ext2") == 0) {
#ifdef EXT2_ENABLED
	    err_printf("Root file system is ext2\n");
	    root = EXT2;
#else
	    err_printf("You have set K42_ROOT_FS = ext2, but not compiled "
		       "support, so I will use NFS as root filesystem.\n");
#endif //#ifdef EXT2_ENABLED
	} else {
	    err_printf("You have set K42_ROOT_FS = `%s', which I do not "
		       "recognize, so I will use NFS as root filesystem.\n",
		       buf);
	}
    }
    return root;
}

#ifdef KFS_ENABLED
// runs as a clone() thread, so must exit() at end
void
RunInitKFS(uval rcPtr)
{
    err_printf("Starting RunInitKFS\n");

    SysStatus rc = InitKFS();

    RootType root = getRoot();
    if (root == KFS) { // passert if failure is for root, no point in going on
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == ENOENT) {
		passertWrn(0, "If you want KFS as root, you need to have "
			   " the environment variable K42_FS_DISK with kfs "
			   " mount specs (see examples on victim.conf).\n");
	    } else {
		passertMsg(0, "InitKFS failed with rc 0x%lx\n", rc);
	    }
	}
    } else if (_FAILURE(rc)) {
	if (_SGENCD(rc) == ENOENT) { // no spec provided
	    tassertWrn(0, "KFS enabled, but no kfs mount specification "
		       "has been provided in environment variable "
		       "KFS_FS_DISK. For examples of the specification "
		       "format, see file victim.conf.\n");
	    rc = 0;
	} else {
	    tassertWrn(0, "InitKFS failed with rc 0x%lx, but let's proceeed "
		       "with initializations\n", rc);
	    rc = 0;
	}
    }
    (*(SysStatus*) rcPtr) = rc;
    _exit(0);
}

/* InitKFS returns 0 if it manages to mount the KFS file systems.
 * It retuns an error with _SGENCD == ENOENT if there is no specification
 * of KFS file system to be mounted (specifications are retrieved from
 * an environment variable; default value obtained from kvictim.pl
 */
SysStatus
InitKFS()
{
    SysStatus rc, rctmp = _SERROR(2827, 0, ENOENT), rcroot = 0;
    uval classInitDone = 0;

    RootType root = getRoot();
    uval have_root = (root == KFS ? 0 : 1);

    DiskMountInfoList diskInfo;
    rc = diskInfo.init();
    if (_SUCCESS(rc)) {
	void *curr = NULL;
	DiskMountInfo *elem;
	while ((curr = diskInfo.next(curr, elem))) {
	    if (strncmp(elem->getFSType(), "kfs", 3) != 0) {
		continue;
	    }
	    char *dev = elem->getDev();
	    char *mnt = elem->getMountPoint();
	    char *flagspec = elem->getFlagSpec();

	    if (!classInitDone) {
		// class initialization is invoked once
		FileSystemKFS::ClassInit();
		classInitDone = 1;
	    }

	    // check if device is ready
	    SysStatus rcdev;
	    FileLinux::Stat stat_buf;

	    rcdev = FileLinux::GetStatus(dev, &stat_buf);

	    if (_FAILURE(rcdev)) {
		err_printf("Cannot open device %s.  Not mounting %s\n",
			   dev, mnt);
		continue;
	    }

	    // find the configuration for kfs mouting corresponding to
	    // the specified flag
	    uval i = 0;
	    kfsMountData *kfs  = NULL;
	    while (kfsMounts[i].name != NULL && kfs == NULL) {
		if (strcmp(kfsMounts[i].name, flagspec) == 0) {
		    kfs = &kfsMounts[i];
		}
		i++;
	    }
	    if (!kfs) {
		err_printf("No KFS configuration has been found for %s."
			   " Skipping mounting of %s on %s\n.",
			   flagspec, dev, mnt);
		continue;
	    }

	    if (kfs->flags & ROOT_ONLY  && have_root) {
		continue;
	    }

	    err_printf("It will start KFS with device %s on mount "
		       "point %s (pid %ld)\n", dev, mnt,
		       _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

	    if (kfs->format == FORMAT) {
		rctmp = FileSystemKFS::CreateAndFormat(dev, mnt,
						       kfs->fsFlags,
						       (kfs->flags&NOPRUNING ? 0:1)/*isCoverable*/);
	    } else {
		rctmp = FileSystemKFS::Create(dev, mnt, kfs->fsFlags,
					      (kfs->flags&NOPRUNING ? 0:1) /*isCoverable*/);
	    }

	    if (_FAILURE(rctmp)) {
		tassertWrn(0, "KFS failed to start, probably device problem. "
			   "rctmp is 0x%lx, device %s, mntPoint %s\n", rctmp,
			   dev, mnt);
		if (kfs->flags & ROOT_ONLY) {
		    // we want to return error about mouting root
		    rcroot = rctmp;
		    break;
		} else {
		    continue;
		}
	    }

	    if (kfs->flags & DO_BINDINGS_ROOT_ONLY && root != KFS) {
		// skip bindings
		continue;
	    }

	    PathNameDynamic<AllocGlobal> *oldPath, *newPath;
	    uval oldLen, newLen;
	    for (uval j = 0; kfs->bindings[j].from ; ++j) {
		oldLen = PathNameDynamic<AllocGlobal>::Create
		    (kfs->bindings[j].from, strlen(kfs->bindings[j].from),
		     0, 0, oldPath);
		if (_FAILURE(oldLen)) {
		    tassertMsg(0, "GetAbsPath failed\n");
		    continue;
		}
		newLen = PathNameDynamic<AllocGlobal>::Create
		    (kfs->bindings[j].to, strlen(kfs->bindings[j].to),
		     0, 0, newPath);
		if (_FAILURE(newLen)) {
		    tassertMsg(0, "GetAbsPath failed\n");
		    continue;
		}

		// FIXME: for now bindings have isCoverable == 1
		rc = StubMountPointMgr::_Bind(oldPath->getBuf(),
					      oldLen,
					      newPath->getBuf(),
					      newLen, 1/*isCoverable*/);
		tassertMsg(_SUCCESS(rc), "ops, _Bind failed");
		oldPath->destroy(oldLen);
		newPath->destroy(newLen);
	    }
	}

    }
    return rctmp;
}
#endif /* KFS_ENABLED */

// runs as a clone() thread, so must exit() at end
void
StartNFS(uval rcPtr)
{
    int ret;

    err_printf("Started StartNFS\n");

    SysStatus rc = FileSystemNFS::ClassInit(0);
    passertMsg(_SUCCESS(rc), "NFS ClassInit failed: %lx\n",rc);

    FILE* nfsMounts;

    char buf[64] = {0,};
    rc = StubKBootParms::_GetParameterValue("K42_SKIP_NFS", buf,
					    64);
    if (!(_FAILURE(rc) || buf[0] == 0)) {
	// K42_SKIP_NFS has been defined on the environment (as non-empty)
	goto return_exit;
    }

    err_printf("Checking for extra NFS mounts in file /knfs/ketc/k42nfs\n");
    nfsMounts = fopen("/knfs/ketc/k42nfs", "r");

    if (nfsMounts) {
	for (;;) {
	    char line[1024];
	    char host[256];
	    char path[256];
	    char mnt[256];

	    if (fgets(line, sizeof(line), nfsMounts) == NULL) {
		break;
	    }
	    // skip comments and blank lines
	    if ((line[0] == '#') || (line[0] == '\n')) {
		continue;
	    }

	    ret = sscanf(line, "%250s %250s %250s\n", host, path, mnt);
	    if (ret == 3) {
		err_printf("NFS mounting %s:%s \n\t-> %s\n", host, path, mnt);
		FileSystemNFS::Create(host, path, mnt, -1, -1,
				      0/*isCoverable*/);
	    } else {
		err_printf("Bad line in k42nfs:  %s", line);
	    }
	}

	fclose(nfsMounts);
    }

return_exit:
    (*(SysStatus*) rcPtr) = 0;
    _exit(0);
}

#ifdef EXT2_ENABLED
// runs as a clone() thread, so must exit() at end
void
StartExt2(uval rcPtr)
{
    SysStatus rc, rctmp = 0;

    err_printf("In StartExt2\n");

    int classInitDone = 0;

    DiskMountInfoList diskInfo;
    rc = diskInfo.init();
    if (_SUCCESS(rc)) {
	void *curr = NULL;
	DiskMountInfo *elem;
	while ((curr = diskInfo.next(curr, elem))) {
	    if (strncmp(elem->getFSType(), "ext2", 4) != 0) {
		continue;
	    }

	    if (!classInitDone) {
		// class initialization is invoked once
		FileSystemExt2::ClassInit(0);
		classInitDone = 1;
	    }

	    // ignoring flags for now
	    rctmp = FileSystemExt2::Create(elem->getDev(),
					   elem->getMountPoint(),
					   0 /*isCoverable*/);
	    tassertMsg(_SUCCESS(rctmp), "rctmp is 0x%lx\n", rctmp);
	}
    }

    (*(SysStatus*)rcPtr) = rctmp;
    _exit(0);
}
#endif // #ifdef EXT2_ENABLED

/*
 * Handles an entry in nfsMountData for binding
 * If the entry does not have the DO_BINDINGS_ROOT_ONY flag set, then does
 *	the regular bindings.  This is basically a no-op since those entries
 *	do not have bindings.
 * If the entry has the DO_BINDINGS_ROOT_ONLY flag set and if the variable
 *	K42_NFS_BINDINGS is set, then do the binding from the variable.  The
 * 	format is
 *	 K42_NFS_BINDINGS="/knfs/klib=/klib,/knfs/kbin=/kbin"
 * If the entry has the DO_BINDINGS_ROOT_ONLY flag set and if the variable
 *	K42_NFS_BINDINS is not set, or it is null, then do the default 
 *	bindings.
 */
static void doNFSBindings(struct nfsMountData* nfs)
{
    if (nfs->flags & DO_BINDINGS_ROOT_ONLY) {
	char bindparam[256];
	SysStatus rc;
	rc = StubKBootParms::_GetParameterValue("K42_NFS_BINDINGS", bindparam,
			 256);
	if (_SUCCESS(rc) && bindparam[0] != '\0' ) {
	    /* Use NFS_BINDINGS parameter instead */
	    err_printf(
		"Skipping default bindings and doing parameter nfs bindings\n");

	    char *from, *to, *next;

	    for (from = bindparam; from && *from; from = next) {
		if (!(to = strchr(from, '='))) {
		    tassertWrn(0, "K42_NFS_BINDINGS parameter invalid: '%s'",
				 from);
		    return;
		}
		*(to++) = '\0'; 
		//if (strcmp(from, nfs->mntPoint)) continue;

		if ((next = strchr(to, ','))) *(next++) = '\0';
		MountPointMgrImp::Bind(from, to, 1/*isCoverable*/);
	    }
	    return;
	}
    }

    /* Do the bindings specified in the nfsMountData structure */
    for (uval i = 0; nfs->bindings[i].from ; ++i) {
	    MountPointMgrImp::Bind(nfs->bindings[i].from,
		    nfs->bindings[i].to, 1);
    }
}

void StartNFSServices()
{
    SysStatus rc;
    char buf[64] = {0,};
    rc = StubKBootParms::_GetParameterValue("K42_SKIP_NFS", buf, 64);
    if (!(_FAILURE(rc) || buf[0] == 0)) {
	// K42_SKIP_NFS has been defined on the environment (as non-empty)
	return;
    }

    RootType root = getRoot();

    uval have_root;

    StubKBootParms::_GetParameterValue("K42_PKGROOT", k42_pkgroot, 64);
    StubKBootParms::_GetParameterValue("K42_PKGHOST", k42_pkghost, 64);

    rc = FileSystemNFS::ClassInit(0);
    passertMsg(_SUCCESS(rc), "NFS ClassInit failed: %lx\n",rc);

    have_root = (root == NFS ? 0 : 1);
    for (uval index = 0; nfsMounts[index].flags != NFS_INVALID; ++index) {
	struct nfsMountData* nfs = &nfsMounts[index];
	if (nfs->flags & ROOT_ONLY  && have_root) {
	    continue;
	}
	if (nfs->host && nfs->host[0] == 0
	    || nfs->path && nfs->path[0] == 0) {
	    tassertWrn(0,"NFS: mount in nfsMounts index %ld is being skipped "
		       "(either host or path is an empty string)\n", index);
	    continue;
	}

	rc = FileSystemNFS::Create(nfs->host, nfs->path,
				   nfs->mntPoint, -1, -1,
				   (nfs->flags&NOPRUNING ? 0:1) /*isCoverable*/);
	if (_FAILURE(rc)) {
	    if (strncmp(nfs->mntPoint, "/knfs", strlen("/knfs")) == 0
		|| strncmp(nfs->mntPoint, "/", strlen("/")) == 0) {
		// important points in the name space, assert
		passertMsg(0, "NFS mount failed for host %s, path %s, "
			   "mpath %s (rc 0x%lx)\n", nfs->host, nfs->path,
			   nfs->mntPoint, rc);
	    } else {
		tassertWrn(0, "NFS mount failed for host %s, path %s, "
			   "mpath %s (rc 0x%lx)\n", nfs->host, nfs->path,
			   nfs->mntPoint, rc);
	    }
	    // skip the binding
	    continue;
	}
	if (_SUCCESS(rc) && nfs->flags & ROOT_ONLY) {
	    have_root = 1;
	}
	if (!(nfs->flags & DO_BINDINGS_ROOT_ONLY) || root == NFS) 
	  doNFSBindings(nfs);
    }
}

void StartFileSystemServices()
{
    StartNFSServices();

    FileSystemK42RamFS::ClassInit(0);

    err_printf("ramFS file system - starting on /ram\n");
    FileSystemK42RamFS::Create(0, "/ram", 0 /*isCoverable*/);

    /*
     * These file systems allow us to run gentoo sshd
     */
    err_printf("ramFS file system - starting on /var/run\n");
    FileSystemK42RamFS::Create(0, "/var/run", 0 /*isCoverable*/);

    err_printf("ramFS file system - starting on /var/lock\n");
    FileSystemK42RamFS::Create(0, "/var/lock", 0 /*isCoverable*/);

    err_printf("ramFS file system - starting on /var/log\n");
    FileSystemK42RamFS::Create(0, "/var/log", 0 /*isCoverable*/);

    err_printf("ramFS file system - starting on /var/tmp\n");
    FileSystemK42RamFS::Create(0, "/var/tmp", 0 /*isCoverable*/);

    err_printf("ramFS file system - starting on /var/empty\n");
    FileSystemK42RamFS::Create(0, "/var/empty", 0 /*isCoverable*/);

    err_printf("ramFS file system - starting on /var/lib\n");
    FileSystemK42RamFS::Create(0, "/var/lib", 0 /*isCoverable*/);

    /*
     * This filesystem allows us to run the SDET benchmark without
     * accessing / and /root from the package directory over NFS.
     */
    err_printf("ramFS file system - starting on /root\n");
    FileSystemK42RamFS::Create(0, "/root", 0 /*isCoverable*/);
}
