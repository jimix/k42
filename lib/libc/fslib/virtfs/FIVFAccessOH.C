/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinux.H>
#include "FileInfoVirtFS.H"
#include "FIVFAccessOH.H"

/* virtual */ SysStatus
FIVFAccessOH::openCreateServerFile(ServerFileRef &fref,
				   uval oflag, ProcessID pid, ObjectHandle &oh,
				   uval &useType, TypeID &ohType)
{
    SysStatus rc = 0;
    if (obj.valid()) {
	rc = Obj::GiveAccessByClient(obj, oh, pid);
	_IF_FAILURE_RET(rc);
    }
    ohType = type;
    useType = FileLinux::NON_SHARED;
    return rc;
}

/* virtual */ SysStatus
FIVFAccessOH::deleteFile()
{
    tassertWrn(0,"Don't know how to do deleteFile in FIVFAccessOH\n");
    return 0;
}


/* virtual */ SysStatus
FIVFAccessOH::createServerFileBlock(ServerFileRef &fref)
{
    tassertMsg(status.isFile() || status.isBlock(),
	       "Creating block file on non-block entity\n");

    //No ServerFile created, but that's ok --- caller should figure
    // out they should not have asked for a ServerFile (DirLinuxFS)
    fref = NULL;
    return 0;
}

/* virtual */ SysStatus
FIVFAccessOH::createServerFileChar(ServerFileRef &fref)
{
    tassertMsg(status.isChar(),
	       "Creating char file on non-char entity\n");

    //No ServerFile created, but that's ok --- caller should figure
    // out they should not have asked for a ServerFile (DirLinuxFS)
    fref = NULL;
    return 0;
}

/* virtual */ SysStatus
FIVFAccessOH::destroy()
{
    Obj::ReleaseAccess(obj);
    return FileInfoVirtFS::destroy();
}


