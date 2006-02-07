/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileKFSK42.C,v 1.14 2004/10/14 15:12:42 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>
#include "ServerFileDirKFS.H"
#include "ServerFileBlockKFS.H"
#include "LSOBasicDir.H"
#include <mem/FR.H>

/* static */ uval FSFileKFS::UsesPhysAddr = 1;

/* static */ SysStatus
FSFileKFS::PageNotFound()
{
    return FR::PAGE_NOT_FOUND;
}

/* virtual */ SysStatus
FSFileKFS::createDirLinuxFS(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    DirLinuxFSRef par)
{
    SysStatus retvalue;
    KFSStat stat;
    LSOBasic *lso;

    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    lso->getAttribute(&stat);
    tassertMsg(S_ISDIR(stat.st_mode), "not a dir.\n");

    retvalue = ServerFileDirKFS::Create(rf, pathName, pathLen, this, par);
    return (retvalue);
}


/*
 * createServerFileBlock()
 *
 *  Create a server file object to represent this block file.
 */
SysStatus
FSFileKFS::createServerFileBlock(ServerFileRef &ref)
{
    LSOBasic *lso;
    KFSStat stat;
    SysStatus rc;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createServerFileBlock() IN\n");

    // Interface semantics require an update to fileInfo
    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    lso->getAttribute(&stat);

    // Make sure it is a file
    if (S_ISREG(stat.st_mode) == 0) {
        KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createServerFileBlock(): "
		    "Not a file. Aborting.\n");
        return _SERROR(2353, 0, EISDIR);
    }

    ObjectHandle oh;
    rc = DREF(globals->tref)->getKptoh(oh);
    tassertMsg(_SUCCESS(rc) && oh.valid(), "?");
    rc = ServerFileBlockKFS::Create(ref, this, oh);

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createServerFileBlock() OUT\n");
    return rc;
}


/* detachMultiLink() is part of the destruction protocol for ServerFiles.
 * It is invoked by a ServerFile that learns it has no clients and no
 * parent; its goal is to take the object from the list managed by the
 * multiLinkMgr.
 *
 * The argument ino is needed as a key for searching in the list.
 *
 * It returns: 1 if the file has been successfully removed from the list;
 *             0 if the file is in the list but it can't be removed now
 *             error if the file does not appear in the list.
 */
/* virtual */ SysStatusUval
FSFileKFS::detachMultiLink(ServerFileRef fref, uval ino)
{
    SysStatusUval rc;
    MultiLinkManager::SFHolder *href;
    globals->multiLinkMgr.acquireLock();
    if (globals->multiLinkMgr.locked_search(ino, href)==0) {
	rc = _SERROR(2481, 0, ENOENT);
    } else {
	tassertMsg(href->fref == fref, "Something weird!\n");

	/* lock this SFHolder, so we're guaranteed that there is no in-flight
	 * use of this SFHolder by a getFSFileOrServerFile */
	href->lock.acquire();
	// interact with ServerFile
	rc = DREF(fref)->detachMultiLink();
	tassertMsg(_SUCCESS(rc), "ops rc 0x%lx\n", rc);
	if (_SGETUVAL(rc) == 1) {
	    // ok to detach
	    // no need to release the lock since the entry will be removed,but...
	    href->lock.release();
	    (void) globals->multiLinkMgr.remove(ino);
	} else {
	    tassertMsg(_SGETUVAL(rc)==0, "ops rc 0x%lx\n", rc);
	    href->lock.release();
	}
    }
    globals->multiLinkMgr.releaseLock();
    return rc;
}


/* virtual */ SysStatus
FSFileKFS::getFSFileOrServerFile(
	char *entryName, uval entryLen,
	FSFile **entryInfo, ServerFileRef &ref,
	MultiLinkMgrLock* &lock,
	FileLinux::Stat *status /* = NULL */)
{
    sval rc;
    ObjToken *otok, *dtok = token;
    ObjTokenID otokID;
    LSOBasicDir *lso;

#ifdef KFS_DEBUG
    char tmpname[255];
    memcpy(tmpname, entryName, entryLen);
    tmpname[entryLen] = '\0';
    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::getFSFileOrServerFile(%s) IN\n", tmpname);
#endif // #ifdef KFS_DEBUG

    // locate the correct file in the directory
    lso = (LSOBasicDir *)dtok->getObj(this);
    tassertMsg(lso != NULL, "?");
    rc = lso->matchDir(entryName, entryLen, &otokID);
    if (rc < 0) {
        *entryInfo = NULL;
        return _SERROR(2494, 0, ENOENT);
    }

    // Set the correct disk partition
    otok = new ObjToken(otokID, globals);
    *entryInfo = new FSFileKFS(globals, otok);
    if (status == NULL) {
	tassertMsg(0, "?");
	return -1;
    }
    reValidateToken(otok, status);
    if ((status->st_nlink > 1) && status->isFile()) {
	MultiLinkManager::SFHolder *href;
	globals->multiLinkMgr.acquireLock();
	if (globals->multiLinkMgr.locked_search((uval)otok, href)==0) {
	    // create server file and add
	    ObjectHandle oh;
	    SysStatus rc = DREF(globals->tref)->getKptoh(oh);
	    tassertMsg(_SUCCESS(rc) && oh.valid(), "?");
	    ServerFileBlockKFS::Create(ref, *entryInfo, oh);
				       
	    href = MultiLinkManager::AllocHolder(ref);
	    globals->multiLinkMgr.locked_add((uval)otok, href);
	} else {
	    ref = href->fref;
	}
	lock = &href->lock;
	lock->acquire();
	globals->multiLinkMgr.releaseLock();
    }
    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::getFSFileOrServerFile(%s) OUT\n", tmpname);
    return 0;
}


/* virtual */ SysStatus
FSFileKFS::freeServerFile(FSFile::FreeServerFileNode *n) 
{
    return globals->freeList.freeServerFile(n);
}


/* virtual */ SysStatus
FSFileKFS::unFreeServerFile(FSFile::FreeServerFileNode *n) 
{
    return globals->freeList.unFreeServerFile(n);
}
