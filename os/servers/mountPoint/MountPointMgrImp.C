/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrImp.C,v 1.28 2005/01/13 22:33:50 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Service for registering and looking up mount points.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <io/PathName.H>
#include <io/FileLinux.H>
#include <meta/MetaMountPointMgr.H>
#include "MountPointMgrImp.H"
#include <stub/StubSystemMisc.H>
#include <sys/KernelInfo.H>

MountPointMgrImp *MountPointMgrImp::obj;

/* static */ SysStatus
MountPointMgr::_RegisterMountPoint(
    // path we are mounting at
    __inbuf(lenMP) const char *mountPath, __in uval lenMP,
    // name space we are mounting at the above path
    __in ObjectHandle oh,
    // pathname of mount point in server
    __inbuf(lenRP) const char *relPath, __in uval lenRP,
    __inbuf(lenDesc) const char *desc, __in uval lenDesc,
    __in uval isCoverable)
{
    SysStatus rc;

    PathName *mountPathPTH, *relPathPTH;
    rc = PathName::PathFromBuf((char *)mountPath, lenMP, mountPathPTH);
    if (_FAILURE(rc)) {
	return rc;
    }
    rc = PathName::PathFromBuf((char *)relPath, lenRP, relPathPTH);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = MountPointMgrImp::obj->registerMountPoint(mountPathPTH, lenMP, oh,
						   relPathPTH, lenRP,
						   desc, lenDesc,
						   isCoverable);
    return (rc);
}

/* virtual */ SysStatusUval
MountPointMgrImp::readMarshBuf(uval len, char *buf, uval &cur, uval &left)
{
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("Q");
    }
    uval res;
    res = marshBuf.copyToClientBuf(len, buf, cur, left);
    return res;
}

/* static */ SysStatus
MountPointMgr::_Bind(
	__inbuf(oldnamelen)char *oldname,
	__in uval oldnamelen,
	__inbuf(newnamelen)char *newname,
	__in uval newnamelen,
	__in uval isCoverable)
{
    SysStatus rc;
    PathName *oldPath, *newPath;

    rc = PathName::PathFromBuf((char *)oldname, oldnamelen, oldPath);
    _IF_FAILURE_RET(rc);

    rc = PathName::PathFromBuf((char *)newname, newnamelen, newPath);
    _IF_FAILURE_RET(rc);

    rc = MountPointMgrImp::obj->bind(oldPath, oldnamelen, newPath,
				       newnamelen, isCoverable);
    if (_SUCCESS(rc)) {
	char buf1[PATH_MAX+1], buf2[PATH_MAX+1];
	SysStatusUval rclen = oldPath->getUPath(oldnamelen, buf1, PATH_MAX+1,
						0);
	tassertMsg(_SUCCESS(rclen), "ops");
	rclen = newPath->getUPath(newnamelen, buf2, PATH_MAX+1,	0);
	tassertMsg(_SUCCESS(rclen), "ops");
	cprintf("%s bound to %s\n", buf1, buf2);
    }
    return (rc);
}

void
MountPointMgrImp::init()
{
    lock.init();
    /*
     * Note, it doesn't really matter what the top element string value is,
     * we always start searching through its children, and anything that mounts
     * with a null pathname will just override this top element (again with
     * no string)
     */
    mountPointCommon.init();
    marshBuf.init();

    // marshal null state into marshal buffer
    mountPointCommon.marshalToBuf(&marshBuf);
}

void
MountPointMgrImp::remarshalIntoBuffer()
{
    mountPointCommon.marshalToBuf(&marshBuf);
    err_printf("M");
    SysStatusUval rc = StubSystemMisc::_IncrementMountVersionNumber();
    tassertMsg(_SUCCESS(rc), "ops");
}

/* static */ void
MountPointMgrImp::ClassInit()
{
    obj = new MountPointMgrImp();
    obj->init();
    MetaMountPointMgr::init();

    // now tell kernel to initalize version number
    StubSystemMisc::_InitMountVersionNumber();
}

/* static */ void
MountPointMgrImp::Bind(char *oldname, char *newname, uval isCoverable)
{
    PathNameDynamic<AllocGlobal> *oldPath, *newPath;
    uval oldlen, newlen, maxpthlen;
    SysStatus rc;

    rc = FileLinux::GetAbsPath(oldname, oldPath, oldlen, maxpthlen);
    if (_FAILURE(rc)) {
	tassertWrn(0, "Bind in baseserver failed\n");
	return;
    }
    rc = FileLinux::GetAbsPath(newname, newPath, newlen, maxpthlen);
    if (_FAILURE(rc)) {
	tassertWrn(0, "Bind in baseserver failed\n");
	return;
    }

    rc = MountPointMgrImp::obj->bind(oldPath, oldlen, newPath, newlen,
				     isCoverable);
    if (_SUCCESS(rc)) {
	cprintf("%s bound to %s\n", newname, oldname);
    }

}


/* static */ SysStatusUval
MountPointMgr::_ReadMarshBuf(__in uval len, __outbuf(__rc:len) char *buf,
			     __inout uval &cur, __out uval &left)
{
    return MountPointMgrImp::obj->readMarshBuf(len, buf, cur, left);
}

/* static */ SysStatus
MountPointMgr::_PrintMtab()
{
    return MountPointMgrImp::obj->printMtab();
}
