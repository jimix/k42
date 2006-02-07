/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSGlobals.C,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "Disk.H"
#include "KFSGlobals.H"

KFSGlobals::KFSGlobals()
{
    disk_ar = NULL;
    recordMap = NULL;
    soAlloc = NULL;
    super = NULL;
    blkCache = NULL;
    llpso = NULL;
#ifdef GATHERING_STATS
    st.initStats();
#endif //#ifdef GATHERING_STATS

#ifndef KFS_TOOLS
    multiLinkMgr.init();
    freeList.init();
#endif // KFS_TOOLS
}
