/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileKFSTools.C,v 1.1 2004/02/24 20:22:31 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "FSFileKFS.H"

/* static */ uval FSFileKFS::UsesPhysAddr = 0;

// The following is not used from the tools side, so it can return whatever
// value.
/* static */ SysStatus
FSFileKFS::PageNotFound()
{
    return 1;
}

/* virtual */ SysStatus
FSFileKFS::createDirLinuxFS(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    DirLinuxFSRef par)
{
    return -1;
}

SysStatus
FSFileKFS::createServerFileBlock(ServerFileRef &ref)
{
    return -1;
}

/* virtual */ SysStatusUval
FSFileKFS::detachMultiLink(ServerFileRef fref, uval ino)
{
    return 0;
}


/* virtual */ SysStatus
FSFileKFS::getFSFileOrServerFile(
	char *entryName, uval entryLen,
	FSFile **entryInfo, ServerFileRef &ref,
	MultiLinkMgrLock* &lock,
	FileLinux::Stat *status /* = NULL */)
{
    return -1;
}


/* virtual */ SysStatus
FSFileKFS::freeServerFile(FSFile::FreeServerFileNode *n) 
{
    return -1;
}


/* virtual */ SysStatus
FSFileKFS::unFreeServerFile(FSFile::FreeServerFileNode *n) 
{
    return -1;
}
