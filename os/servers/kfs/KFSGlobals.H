#ifndef __KFSGLOBALS_H_
#define __KFSGLOBALS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSGlobals.H,v 1.14 2005/07/10 16:04:57 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the state global to KFS, have in one
 * spot so we can figure out different ways to get to this if strategy
 * changes.
 * **************************************************************************/

class Disk;
class PSOBase;
class RecordMap;
class ServerObjectAllocator;
class SuperBlock;
class BlockCache;

#ifndef KFS_TOOLS
#include <fslib/FSStats.H>
#include <fslib/MultiLinkManager.H>
#include <fslib/FreeList.H>
#include <fslib/PagingTransport.H>
#endif // KFS_TOOLS

// need to include this crap because of template
#include "KFSHash.H"
#include "ServerObject.H"


class KFSGlobals {
    uval _isSyncMetaDataOn;
public:
    Disk *disk_ar;
    RecordMap *recordMap;
    ServerObjectAllocator *soAlloc;
    // FIXME: with some work (not trivial, we thing), we can can get rid of
    // the following hash. If all accesses came through KFSFile, things
    // would be easy, but some operations reach for other objects beyond
    // the one associated with its KFSFile
    KFSHash<uval64, ServerObject *> soHash;
    // FIXME: get rid of this too
    SuperBlock *super;
    BlockCache *blkCache;
    // low-level PSO, for accessing disk
    PSOBase *llpso;

#ifndef KFS_TOOLS
    // generic objects all file systems know about
    FSStats st;
    MultiLinkManager multiLinkMgr;
    FreeList freeList;

    // for communication with MM
    PagingTransportRef tref;

    DEFINE_GLOBALPADDED_NEW(KFSGlobals);
#endif // KFS_TOOLS

    KFSGlobals();
    uval isSyncMetaDataOn() { return _isSyncMetaDataOn; }
    void setSyncMetaData(uval value) {
	tassertMsg(value == 0 || value == 1, "value is %ld\n", value);
	_isSyncMetaDataOn = value; 
    }
};

#endif /* #ifndef __KFSGLOBALS_H_ */
