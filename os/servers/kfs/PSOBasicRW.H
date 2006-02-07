#ifndef __PSOBASIC_RW_H_
#define __PSOBASIC_RW_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: PSOBasicRW.H,v 1.32 2004/09/15 20:47:18 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *		  Hurricane File Manager : BasicRWObj.h
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

#include "SuperBlock.H"
#include "ObjToken.H"
#include "PSOBase.H"
#include "KFSDebug.H"
#include "BlockCache.H"

// max blocks in the direct block list
// The count here should be:
//   (OS_BLOCK_SIZE - sizeof(blockSize)) / sizeof(uval32)
//   (4096 - 4) / 4
#define RW_MAXBLK 1023

#define PSO_BASICRW_NONE  0x00
#define PSO_BASICRW_DIRTY 0x01

/*
 * A basic physical server object (PSO) for read/write data where
 * object data is equivalent to a Unix indirect block
 */
class PSOBasicRW : public PSOBase {
protected:
    PSOBase *llpso;    // lower-level PSO, to be used for IO
    uval32 blkno;      // the physical block nb for the metadata of this PSO
    uval32 flags;

    // the buffer of the metadata
    BlockCacheEntry *data;

    // the possible sub-object of this pso
    ObjToken subObj;

    // the direct pointers to the data
    // FIXME: based on block size!  64 bit? only 2 terabytes w/ 32bit
    uval32 *dblk;          // has [RW_MAXBLK] entries
    ObjTokenID *subObjID;  // pointer do data.id

    // functions which assume that the PSO's lock is already held.
    void locked_flush();
    sval locked_freeBlocks(uval32 fromLBlk, uval32 toLBlk, uval inUnlink=0);
    void locked_unlink();
    void locked_markDirty(uval32);
    virtual uval32 getDblk(uval32 lblkno, uval8 create=0);

    // layout of the data block
    struct PSOBasicRWBlock {
        ObjTokenID id;              // ID of the subObj
        uval32     dBlk[RW_MAXBLK];
    };

public:
    DEFINE_GLOBAL_NEW(PSOBasicRW);

    // constructors
    PSOBasicRW(PSOBase *p, KFSGlobals *g)
	: PSOBase(g), llpso(p), flags(PSO_BASICRW_NONE), subObj(g) {
	passertMsg(0, "fix endian issues in this class before trying to use "
		   "it\n");
	data = NULL;
	dblk = NULL;
	subObjID = NULL;
    }

    PSOBasicRW(ObjTokenID, FSFileKFS *f, PSOBase *p, KFSGlobals *g);

    ~PSOBasicRW();

    // handle the reading and writing of data
    sval readBlock(uval32 lblkno, char *buffer,
		   PSOBase::AsyncOpInfo *cont = NULL,
		   uval isPhysAddr = 0);
    sval writeBlock(uval32 lblkno, char *buffer,
		    PSOBase::AsyncOpInfo *cont = NULL);
    sval readBlockPhys(uval32 lblkno, char *buffer,
		       PSOBase::AsyncOpInfo *cont = NULL);

    // block-cache integration stuff
    BlockCacheEntry *readBlockCache(uval32 b);

    void freeBlockCache(BlockCacheEntry *entry) {
	llpso->freeBlockCache(entry);
    }

    virtual SysStatus writeBlockCache(BlockCacheEntry *block, uval32 lblkno);

    // free the blocks allocated to this PSO
    sval freeBlocks(uval32 fromLBlk, uval32 toLBlk);

    // delete the PSO, and all associated data
    void unlink();

    // flush a dirty PSO to disk
    void flush();

    // mark the PSO as dirty
    void markDirty(uval32);
    SysStatusUval isDirty() { return flags & PSO_BASICRW_DIRTY; }

    // extra functionality for user-created PSOs
    sval special(sval, void *);

    // creates a new PSO with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID, FSFileKFS *f);

    // allocates disk space for the PSO and returns its location
    SysStatusUval locationAlloc(ObjTokenID);
};

#endif /* #ifndef __PSOBASIC_RW_H_ */
