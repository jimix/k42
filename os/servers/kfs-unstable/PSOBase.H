#ifndef __PSOBASE_H_
#define __PSOBASE_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: PSOBase.H,v 1.2 2004/05/05 19:57:59 lbsoares Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *		  Hurricane File Manager : BSObj.h
 *			     Copyright 1993
 *
 *		    	   Authors: Orran Krieger
 *
 * 	This software is free for all non-commercial use, and anyone in this
 * category may freely copy, modify, and redistribute any portion of it,
 * provided they retain this disclaimer and author list.
 * 	The authors do not accept responsibility for any consequences of
 * using this program, nor do they in any way guarantee its correct operation.
 *****************************************************************************/

#include "ServerObject.H"

class BlockCacheEntry;

#define OS_BLOCK_SHIFT	12
#define OS_BLOCK_SIZE	(1UL << OS_BLOCK_SHIFT)
#define OS_BLOCK_MASK	(~(OS_BLOCK_SIZE-1))

#define OS_SECTOR_SIZE  (0x200)

#define PSO_EXTERN 0
#define PSO_LOCAL  1

/*
 * The PSOBase class is the base set of interfaces which every
 * Physical Server Object (PSO) must support.
 */
class PSOBase : public ServerObject {
public:
    PSOBase(KFSGlobals *g):ServerObject(g) { }
    PSOBase(ObjTokenID *id, RecordMapBase *r, KFSGlobals *g) :
	ServerObject(id, r, g) { }

    // handle the reading and writing of data
    virtual sval readBlock(uval32 lblkno, char *buffer, uval local,
			   uval isPhysAddr = 0) = 0;
    virtual sval writeBlock(uval32, char *, uval) = 0;
    virtual sval readBlockPhys(uval32, char *, uval) {
	passertMsg(0, "why is this being used?");
	return 0;
    }

    // read routine should (optionally) handle memory allocation for
    // the client. This is useful for efficiently using block caches
    virtual BlockCacheEntry *readBlockCache(uval32 b, uval local) = 0;
    virtual void freeBlockCache(BlockCacheEntry *) = 0;
    virtual SysStatus writeBlockCache(BlockCacheEntry *block, uval32 lblkno) = 0;

    // free the blocks allocated to this PSO
    virtual sval freeBlocks(uval32, uval32) = 0;

    // delete the PSO, and all associated data
    virtual void unlink() = 0;

    // flush a dirty PSO to disk
    virtual void flush() = 0;

    // extra functionality for user-created PSOs
    virtual sval special(sval, void *) = 0;
};

#endif /* #ifndef __PSOBASE_H_ */
