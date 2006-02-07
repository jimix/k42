/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemUnion.C,v 1.3 2004/01/07 21:11:38 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include "NameTreeLinuxUnion.H"
#include <sys/MountPointMgrClient.H>

#include "FileSystemUnion.H"

static ThreadID BlockedThread = Scheduler::NullThreadID;

/* static */ void
FileSystemUnion::Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

/* static */ SysStatus
FileSystemUnion::ClassInit(VPNum vp, char *primPath, char *secPath,
			   char *mpath, uval isCoverable /* = 1 */)
{
    if (vp != 0) {
	return 0;
    }

    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;  
    SysStatus rc;
    ObjectHandle primOH, secOH;

    char p1[PATH_MAX+1], p2[PATH_MAX+1]; // unix path for description of mp
    uval p1len, p2len;

    // get object handles for primary and secondary paths in this union
    // file system
    rc = FileLinux::GetAbsPath(primPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	tassertMsg(0, "GetAbsPath(%s) failed", primPath);
	return rc;
    }
    p1len = pth->getUPath(pthlen, p1, sizeof(p1));

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, primOH);
    if (_FAILURE(rc)) {
	tassertMsg(0, "MountPointMgr lookup for %s failed rc=(%ld, %ld, %ld)\n",
		   primPath, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	pth->destroy(maxpthlen);
	return rc;
    }
    p2len = pth->getUPath(pthlen, p2, sizeof(p2));

    rc = FileLinux::GetAbsPath(secPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	tassertMsg(0, "GetAbsPath(%s) failed", primPath);
	return rc;
    }
    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, secOH);
    if (_FAILURE(rc)) {
	tassertMsg(0, "MountPointMgr lookup for %s failed rc=(%ld, %ld, %ld)\n",
		   secPath, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	pth->destroy(maxpthlen);
	return rc;
    }

    char tbuf[2*(PATH_MAX+1) + 32];
    sprintf(tbuf, "unionfs of %s and %s", p1, p2);
    
    NameTreeLinuxUnion::Create(mpath, primOH, secOH, tbuf, strlen(tbuf),
			       isCoverable);

    return 0;
}

/* static */ SysStatus
FileSystemUnion::_Mkfs(char *primPath, char *secPath, char *mpath,
		       uval isCoverable)
{
    return ClassInit(0, primPath, secPath, mpath, isCoverable);
}

