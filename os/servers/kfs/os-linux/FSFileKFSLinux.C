/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileKFSLinux.C,v 1.8 2004/03/07 00:42:40 lbsoares Exp $
 *****************************************************************************/

#include "defines.H"
#include "FSFileKFS.H"
#include "LSOBasicDir.H"

/* static */ uval FSFileKFS::UsesPhysAddr = 0;

// The following is not used from the Linux side, so it can return whatever
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
    sval rc;
    ObjToken *otok;
    ObjTokenID otokID;
    LSOBasicDir *lso;

    // locate the correct file in the directory
    lso = (LSOBasicDir *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    rc = lso->matchDir(entryName, entryLen, &otokID);
    if (rc < 0) {
        *entryInfo = NULL;
        return _SERROR(2350, 0, ENOENT);
    }

    otok = new ObjToken(otokID, globals);
    *entryInfo = new FSFileKFS(globals, otok);

    return 0;
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
