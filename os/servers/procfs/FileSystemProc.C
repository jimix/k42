/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemProc.C,v 1.11 2005/07/13 17:27:43 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to /proc
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fslib/NameTreeLinuxFS.H>
#include <stub/StubProcessLinuxServer.H>
#include <stub/StubBuildDate.H>
#include "FileSystemProc.H"
#include <time.h>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <fslib/virtfs/FIVFAccessOH.H>
#include <fslib/DirLinuxFSVolatile.H>
#include <fslib/NameTreeLinuxFSVirtFile.H>
#include "ProcFileTemplate.H"
#include "ProcFileMeminfo.H"
#include "ProcFileCpuinfo.H"
#include "ProcFileMounts.H"
#include <sys/utsname.h>
#include <linux/limits.h>
#include <stdio.h>


/* 
 * The class FileInfoVirtFSProcDir catches failure to lookup (open, stat,
 * etc., against a file system.  We use this for /proc to know which
 * entries are used that are not yet implemented.
 */
/* virtual */ SysStatus
FileInfoVirtFSProcDir::locked_lookup(const char *name, uval namelen, 
				     DirEntry &entry)
{
    SysStatus rc = FileInfoVirtFSDirStatic::locked_lookup(name, namelen, entry);
    
#if 0
    if (_FAILURE(rc)) {
        char buf[256];
	memcpy(buf, name, namelen);
	buf[namelen] = '\0';
        tassertWrn(_SUCCESS(rc),
	      	   "proc fs lookup failed on component '%s' with rc %lx\n", 
	           name, rc);
    }
#endif
    return rc;
}

const char* FileSystemProc::Root = "/proc";
FileSystemProc *FileSystemProc::theFileSystemProc = NULL;

/* static */ SysStatus
FileSystemProc::Create(FileSystemProc* &fs, const char *mpath, mode_t mode)
{
    SysStatus rc;
    fs = new FileSystemProc();
    rc = fs->init(mode);
    if (_SUCCESS(rc)) {
	fs->mountToNameSpace(mpath);
    } else {
	delete fs;
    }

    return rc;
}

/* virtual */ SysStatus
FileSystemProc::mountToNameSpace(const char *mpath)
{
    FileLinux::Stat stat;

    DirLinuxFSRef dref;
    SysStatus rc = DirLinuxFSVolatile::CreateTopDir(dref, ".", this);
    _IF_FAILURE_RET(rc);

    rc = lockGetStatus(&stat);
    _IF_FAILURE_RET(rc);

    NameTreeLinuxFSVirtFile::Create(mpath, dref,
		 1); /* show any failures to access files or directories */

    locked_setParent(this);
    stat.st_nlink=1;
    unlockPutStatus(&stat);
    return rc;
}

/* some constants are environment vars defined in the Makefile */
#define UTS_VERSION_1	"#1 SMP "
#define COMPILE_DATE_LENGTH _UTSNAME_RELEASE_LENGTH
#define BUILT_BY_LENGTH _UTSNAME_RELEASE_LENGTH

char linux_uts_version[1024] = UTS_VERSION_1;
char linux_banner[2056] = "Linux Version ";
char linux_ngroups_max[1024];


/* static */ SysStatus
FileSystemProc::ClassInit(uval isCoverable /* = 0 */)
{
    SysStatus rc;
    FileInfoVirtFSBase::ClassInit();
    const mode_t dir_mode = (mode_t) (S_IREAD|S_IEXEC|S_IRGRP|S_IXGRP|
			  	      S_IROTH|S_IXOTH); /* 0555 */


    FileSystemProc* procFS = NULL;
    rc = FileSystemProc::Create(procFS, Root, dir_mode );
    _IF_FAILURE_RET_VERBOSE(rc);

    FileSystemProc::theFileSystemProc = procFS;

    // make basic directories: /proc/sys, /proc/sys/kernel
    FSFile *dirSys, *dirKern;
    rc = procFS->mkdir("sys", strlen("sys"), dir_mode, &dirSys);
    _IF_FAILURE_RET_VERBOSE(rc);

    rc = dirSys->mkdir("kernel", strlen("kernel"), dir_mode, &dirKern);
    _IF_FAILURE_RET_VERBOSE(rc);

    FileSystemProc::init_linux_uts_version();
    FileSystemProc::init_linux_banner();
    FileSystemProc::init_linux_ngroups_max();

    //Insert /proc/version entry
    rc = FileSystemProc::mkStaticTextRO(procFS, "version", linux_banner);
    _IF_FAILURE_RET_VERBOSE(rc);

    // make /proc/meminfo
    ProcFileMeminfo *mfid=NULL;
    mfid = new ProcFileMeminfo();
    rc = procFS->add(mfid->filename, strlen(mfid->filename), mfid);
    _IF_FAILURE_RET_VERBOSE(rc);

    // make /proc/cpuinfo
    ProcFileCpuinfo *cfid=NULL;
    cfid = new ProcFileCpuinfo();
    rc = procFS->add(cfid->filename, strlen(cfid->filename), cfid);
    _IF_FAILURE_RET_VERBOSE(rc);

    // make /procs/mounts, even though for now it is empty
    ProcFileMounts *mtfid=NULL;
    mtfid = new ProcFileMounts();
    rc = procFS->add(mtfid->filename, strlen(mtfid->filename), mtfid);
    _IF_FAILURE_RET_VERBOSE(rc);

    // make /proc/sys/kernel/ngroups_max
    rc = FileSystemProc::mkStaticTextRO(dirKern, "ngroups_max",
			                linux_ngroups_max);
    _IF_FAILURE_RET_VERBOSE(rc);

    /*
     * KEEP this entry last.
     */
    // make /proc/sys/kernel/version
    // keep this last since init code looks for it
    rc = FileSystemProc::mkStaticTextRO(dirKern, "version", linux_uts_version);
    _IF_FAILURE_RET_VERBOSE(rc);

    return rc;
}

/*static*/ SysStatus
FileSystemProc::init_linux_uts_version() {
    char compile_date[COMPILE_DATE_LENGTH];
    StubBuildDate::_getLinkDate(compile_date, COMPILE_DATE_LENGTH);
    strcat(linux_uts_version, compile_date);
    strcat(linux_uts_version, "\n");
    return 0;
}

/*static*/ SysStatus
FileSystemProc::init_linux_banner() {
    char linux_uts_release[_UTSNAME_RELEASE_LENGTH];
    StubProcessLinuxServer::_getLinuxVersion(linux_uts_release,
					 _UTSNAME_RELEASE_LENGTH);
    strcat(linux_banner, linux_uts_release);

    strcat(linux_banner, " (");

    char built_by[BUILT_BY_LENGTH];
    StubBuildDate::_getBuiltBy(built_by, BUILT_BY_LENGTH);
    strcat(linux_banner, built_by);

    strcat(linux_banner, "@" LINUX_COMPILE_HOST ") (" COMPILER_VERSION ") ");
    strcat(linux_banner, linux_uts_version);
    return 0;
}

/*static*/ SysStatus
FileSystemProc::init_linux_ngroups_max() {
    sprintf(linux_ngroups_max, "%d", NGROUPS_MAX);
    return 0;
}

/*static*/ SysStatus
FileSystemProc::mkStaticTextRO(FSFile *dir, char *fname, char *val) {
    SysStatus rc;
    ProcFSFile_StaticTextRO *fid=NULL;
    fid = new ProcFSFile_StaticTextRO(fname, val);
    rc = ((FileInfoVirtFSDirStatic *)dir)->add(fid->filename,
		 strlen(fid->filename), fid);
    return rc;
}
