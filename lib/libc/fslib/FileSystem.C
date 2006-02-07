/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystem.C,v 1.29 2004/07/08 17:15:30 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include  "ServerFile.H"
#include <trace/traceFS.h>
#include "FileSystemGlobal.H"
//#include <stdio.h>

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
FileSystemGlobal::detachMultiLink(ServerFileRef fref, uval ino)
{
    SysStatusUval rc;
    MultiLinkManager::SFHolder *href;
    multiLinkMgr.acquireLock();
    if (multiLinkMgr.locked_search(ino, href)==0) {
	rc = _SERROR(2177, 0, ENOENT);
    } else {
	tassertMsg(href->fref == fref, "Something weird!\n");

	/* lock this SFHolder, so we're guaranteed that there is no in-flight
	 * use of this SFHolder by a getFSFileOrServerFile */
	href->lock.acquire();
	// interact with ServerFile
	rc = DREF(fref)->detachMultiLink();
	tassertMsg(_SUCCESS(rc), "ops\n");
	if (_SGETUVAL(rc) == 1) {
	    // ok to detach
	    // no need to release the lock since the entry will be removed,but...
	    href->lock.release();
	    (void) multiLinkMgr.remove(ino);
	} else {
	    tassertMsg(_SGETUVAL(rc)==0, "ops\n");
	    href->lock.release();
	}
    }
    multiLinkMgr.releaseLock();
    return rc;
}

void
FileSystemGlobal::init()
{
    multiLinkMgr.init();
    freeList.init();
#ifdef GATHERING_STATS
    st.initStats();
#endif //#ifdef GATHERING_STATS
}
