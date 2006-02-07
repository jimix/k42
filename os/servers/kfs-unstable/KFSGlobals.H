#ifndef __KFSGLOBALS_H_
#define __KFSGLOBALS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSGlobals.H,v 1.3 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the state global to KFS, have in one
 * spot so we can figure out different ways to get to this if strategy
 * changes.
 * **************************************************************************/

class Disk;
class PSOBase;
class RecordMapBase;
class ServerObjectAllocator;
class SuperBlock;
class BlockCache;

#ifndef KFS_TOOLS
#include <fslib/FSStats.H>
#include <fslib/MultiLinkManager.H>
#include <fslib/FreeList.H>
#endif // KFS_TOOLS

// need to include this crap because of template
#include "KFSHash.H"
#include "ServerObject.H"

class KFSGlobals {
public:
    Disk *disk_ar;
    RecordMapBase *recordMap;
    ServerObjectAllocator *soAlloc;
    // FIXME: get rid of this too
    SuperBlock *super;
    BlockCache *blkCache;
    // low-level PSO, for accessing disk
    PSOBase *llpso;

#ifndef KFS_TOOLS
    // generic objects all file systems know about
    FSStats st;
    FreeList freeList;
#endif // KFS_TOOLS

    MultiLinkManager multiLinkMgr;

    DEFINE_GLOBALPADDED_NEW(KFSGlobals);

    KFSGlobals();
};

#endif /* #ifndef __KFSGLOBALS_H_ */
