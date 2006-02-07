#ifndef __SUPER_BLOCK_H_
#define __SUPER_BLOCK_H_

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
 * $Id: SuperBlock.H,v 1.60 2005/07/05 16:55:38 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *                    Hurricane File System : Superblock.h
 *                           Copyright 1991
 *
 *                         Authors: Orran Krieger
 *
 * 	This software is free for all non-commercial use, and anyone in this
 * category may freely copy, modify, and redistribute any portion of it,
 * provided they retain this disclaimer and author list.
 * 	The authors do not accept responsibility for any consequences of
 * using this program, nor do they in any way guarantee its correct operation.
 *****************************************************************************/

#include "Byteorder.H"
#include "PSOBase.H"
#include "ObjToken.H"
#include "KFSGlobals.H"
#include <BlockCache.H>
#include <misc/ListSimpleLocked.H>

#ifdef KFS_TOOLS
#include <stdio.h>
#endif

#define SUPER_BLOCK_DIRTY 1

/*
 * Initial disk setup after format:
 *
 * 0      unused (reserved for boot)
 * 1      first superblock
 * 2      second superblock
 * 3:10   block bitmap
 * 11     record map metadata
 * 12     first record map data block
 * 13     root directory PSO metadata
 * 14     first root directory PSO data block
 */
// This structure occupies exactly 1 OS_BLOCK (or less...)

struct SuperBlockStruct
{
    // FIXME: both Dilma and Livio don't understand why we have fsname here ...
    char fsname[32];	  // Name of file system in tree

private:
    uval32 version;       /* keeping track of KFS version used to build
			   * file system */
    uval32 numBlocks __attribute__ ((packed)); // Total number of blocks
    uval32 freeBlocks __attribute__ ((packed));	  // Number of free blocks

    // FIXME: it seems type and state are not being used for anything
    uval32 type __attribute__ ((packed));	  // File system type
    uval32 state __attribute__ ((packed));	  // File system state

    // Record oriented map for storing obj data
    uval32 recMapBlkno __attribute__ ((packed));   
    ObjTokenID rootLSO __attribute__ ((packed));   // the root directory

public:
    /* This field is not used by default. Some
     * specializations of SuperBlock (e.g. the one
     * we're working on for fs consistency) may
     * need to associate a generation number
     * to the superblock. It has to stay in last sector.*/
    uval32 generation __attribute__ ((packed));     

    // note: this should be indexed through (numBlocks/(4096*8) ???)
    // holds physical block # of BlockMap #s
    uval32 blockMapBlocks[0] __attribute__ ((packed)); 


#ifdef KFS_DEBUG_ENDIAN
#define SB_GET_DEBUG32(name, v) err_printf("Get for %s: CPU 0x%x ",\
                                           name, TE32_TO_CPU(v));\
                                PRINT_TE32(v, 1);
#define SB_SET_DEBUG32(name, v) err_printf("Set for %s: CPU 0x%x ",\
                                           name, TE32_TO_CPU(v));\
                                PRINT_TE32(v, 1);
#define SB_GET_DEBUG64(name, v) err_printf("Get for %s: CPU 0x%llx ",\
                                           name, TE64_TO_CPU(v));\
                                PRINT_TE64(v, 1);
#define SB_SET_DEBUG64(name, v) err_printf("Set for %s: CPU 0x%llx ",\
                                           name, TE64_TO_CPU(v));\
                                PRINT_TE64(v,1);
#else // #ifdef KFS_DEBUG_ENDIAN

#define SB_GET_DEBUG32(name, v)
#define SB_SET_DEBUG32(name, v)
#define SB_GET_DEBUG64(name, v)
#define SB_SET_DEBUG64(name, v)

#endif // #ifdef KFS_DEBUG_ENDIAN

    uval32 getVersion() {
	SB_GET_DEBUG32("version", version);
	return TE32_TO_CPU(version);
    }
    void setVersion(uval32 v) {
	version = CPU_TO_TE32(v);
	SB_SET_DEBUG32("version", version);
    }
    uval32 getNumBlocks() {
	SB_GET_DEBUG32("numBlocks", numBlocks);
	return TE32_TO_CPU(numBlocks);
    }
    void setNumBlocks(uval32 n) {
	numBlocks = CPU_TO_TE32(n);
	SB_SET_DEBUG32("numBlocks", numBlocks);
    }
    uval32 getFreeBlocks() { return TE32_TO_CPU(freeBlocks); }
    void setFreeBlocks(uval32 n) { freeBlocks = CPU_TO_TE32(n); }
    uval16 getType() { return TE16_TO_CPU(type); }
    void setType(uval16 t) { type = CPU_TO_TE16(t); }
    uval16 getState() { return TE16_TO_CPU(state); }
    void setState(uval16 s) { state = CPU_TO_TE16(s); }
    uval32 getRecMap() {
	SB_GET_DEBUG32("recMapBlkno", recMapBlkno);
	return TE32_TO_CPU(recMapBlkno);
    }
    void setRecMap(uval32 r) {
	recMapBlkno = CPU_TO_TE32(r);
	SB_SET_DEBUG32("recMapBlkno", recMapBlkno);
    }
    ObjTokenID getRootLSO() {
	ObjTokenID tmp;
	tmp.id = TE32_TO_CPU(rootLSO.id);
#ifdef KFS_DEBUG_ENDIAN
	err_printf("getRootLSO returning id (id 0x%x)\n", tmp.id);
#endif // #ifdef KFS_DEBUG_ENDIAN
	return tmp;
    }
    void setRootLSO(ObjTokenID id) {
	rootLSO.id = CPU_TO_TE32(id.id);
#ifdef KFS_DEBUG_ENDIAN
	err_printf("In setRootLSO id is 0x%x\n", id.id);
	err_printf("setRootSLO set id to ");
	PRINT_TE32(rootLSO.id, 0);
#endif // #ifdef KFS_DEBUG_ENDIAN
    }
    void print();
};

#define CEIL(n, d) (((n) + (d) - 1) / (d))

class BlockMap
{
protected:
    PSOBase *llpso;
    BlockCacheEntry **block; /* the data blocks for the bitmap
			      * - has numBlocks elements */
    uval *dirty;             // dirty flags - has bmapBlocks elements
    
    uval lastAlloc;
    KFSGlobals *globals;
    
public:
    uval numBlocks;
    uval numBits;
    
    DEFINE_GLOBAL_NEW(BlockMap);
    BlockMap(PSOBase *p, KFSGlobals *g, uval num_blocks) : llpso(p) {
	tassertMsg(num_blocks != 0, "?");
	numBlocks = CEIL(num_blocks, 4096 * 8);
	numBits = 8 * OS_BLOCK_SIZE * numBlocks;
	globals = g;
    }

    virtual ~BlockMap() {
	for (uval i = 0; i < numBlocks; i++) {
	    globals->blkCache->freeBlock(block[i]);
        }

	delete dirty;
	delete block;
    }

    // read and write the blockmap
    SysStatus init(uval32 *blocks);
    virtual SysStatus sync();

    // format the bitmap as fresh
    SysStatus format(uval, uval);

    // functions to access the bitmap
    uval32 alloc(uval32 &);
    uval32 allocOne();
    SysStatus free(uval, uval);

    SysStatus sweepBitMap();
    SysStatus copyBitMap(uval8 *bm);
};

/* Primary purpose of this class is to indicate the position of all other
 * per-disk data structures on disk. This is a per-disk object. The superblock
 * is the only per-disk data structure whose location on the disk is fixed.
 * ===========================================================================
 */
class SuperBlock {
protected:
    BLock lock;
    PSOBase *llpso;
    KFSGlobals *globals;
    
    BlockCacheEntry *sblock;        // the data block for the superblock
    SuperBlockStruct *superblock;   // superblock pointer
    BlockMap *bmap;                 // the block map

    uval flags;                     // flags used to mount this file system
    uval dirty;                     // dirty flags

private:
    // The following keeps a list of dirty server objects associaited with this
    // file system. In previous work we did (in the private tree) we kept
    // the list as a hash table, but the pattern of use we have so far
    // fits a simple list
    ListSimpleLocked<ServerObject::DirtySONode*, AllocGlobal> dirtySOList;
public:
    static const uval32 KFS_VERSION = 11;

    static const uval FIRST_SUPER_BLOCK   = 1;
    static const uval SECOND_SUPER_BLOCK  = 2;
    static const uval BLKBIT_BLOCK        = 3;

    DEFINE_GLOBAL_NEW(SuperBlock);

    SuperBlock(PSOBase *p, uval fl, KFSGlobals *g) :
	llpso(p), globals(g), flags(fl) {
	sblock = NULL;
	dirty = 0;
#if 0
	err_printf("Let's play with BE and LE\n");
	uval16 value1 = 17185; // this i 0x4321
	err_printf("value as seen by cpu: 0x%x (%d), value after conversion : ",
		   value1, value1);
	err_printf("print bits of value 1 as if it were TE\n");
	PRINT_TE16(value1, 1);
	uval16 value1con = CPU_TO_TE16(value1);
	err_printf("now after conversion CPU_TO_TE16\n");
	PRINT_TE16(value1con, 1);
	uval16 value1concon = TE16_TO_CPU(value1con);
	err_printf("value1concon is 0x%x(%d)\n", value1concon, value1concon);

	uval32 value2 = 0x44332211;
	err_printf("value as seen by cpu: 0x%x, value after conversion : ",
		   value2);
	PRINT_TE32(CPU_TO_TE32(value2), 1);
	uval64 value3 = 0xaabbccdd11223344;
	err_printf("value as seen by cpu: 0x%llx, value after conversion : ",
		   value3);
	PRINT_TE64(CPU_TO_TE64(value3), 1);
#endif

    }

    virtual ~SuperBlock() {
	err_printf("~SuperBlock()\n");
	globals->blkCache->freeBlock(sblock);
	// free dirtySOList entries
	ServerObject::DirtySONode *n;
	while (dirtySOList.removeHead(n)) {
	    delete n;
	}
    }
    
    void freeBlockMap() {
	delete bmap;
    }

    // Reads in the data from the disk, or get prepared to format
    virtual SysStatus init(uval format = 0);
    
    virtual SysStatus checkVersion() {
	if (superblock->getVersion() == KFS_VERSION) {
	    return 0;
	} else {
	    err_printf("KFS_VERSION %ld, superblock->version %ld\n",
		       (uval) KFS_VERSION, (uval) superblock->getVersion());
	    return _SERROR(2582, 0, EINVAL);
	}
    }

    uval getFlags() { return flags; }

    uval32 getRecMap() { return superblock->getRecMap(); }

    void setRecMap(uval32 blkno) {
	superblock->setRecMap(blkno);
	dirty |= SUPER_BLOCK_DIRTY;
    }

    ObjTokenID getRootLSO() { return superblock->getRootLSO(); }
    
    // setRootLSO is called by the tool creating the file system
    void setRootLSO(ObjTokenID otok) { superblock->setRootLSO(otok); }

    // set BlockMap block mapping
    void setBlockMap(uval index, uval32 blocknr) {
	passertMsg(index <= bmap->numBlocks, "SuperBlock::setBlockMap() "
		   "invalid index! index=%lu, numBlock=%lu\n",
		   index, bmap->numBlocks);
	superblock->blockMapBlocks[index] = CPU_TO_TE64(blocknr);
	dirty |= SUPER_BLOCK_DIRTY;
    }

    // Initializes bit maps & superblock
    virtual SysStatus format(char *fsname);

    SysStatus createRootDirectory();

    void display();
    void displayBlockMap();

    // locked operations
    virtual SysStatus allocBlock();
    virtual SysStatus allocExtent(uval32 &);
    SysStatus freeBlock(uval32);

    virtual void sync();
    // sync operation implementation may need to guarantee that ServerObjects
    // don't go away while it's manipulating them (e.g. invoking flush).
    // An RCU like infra-structure would solve the problem, but for this
    // limited requirement we can get away with SuperBlock::sync locking
    // for write and FSFileKFS destructor (responsible for initiating
    // destruction of ServerObjects) acquiring the lock for read.
    // The current implementation of sync doesn't need this functionality,
    // so the default implementation is null
    virtual void syncLockAcquireR() {}
    virtual void syncLockReleaseR() {}
    virtual void syncLockAcquireW() {}
    virtual void syncLockReleaseW() {}

    SysStatus statfs(struct statfs *buf);
    SysStatus copyBitMap(uval8 *bm);

    // ----- methods to manipulate list dirtySOList ----
    ServerObject::DirtySONode *addDirtySO(ServerObject *so) {
	ServerObject::DirtySONode *dnode = new ServerObject::DirtySONode(so);
	dirtySOList.add(dnode);
	return dnode;
    }

    void flushDirtySOList() {
	ServerObject::DirtySONode *dnode;
	while (dirtySOList.removeHead(dnode)) {
	    tassertMsg(dnode != NULL, "look");
	    dnode->lock.acquire();
	    if (dnode->locked_isValid()) {
		dnode->so->flush();
	    }
	    dnode->so->setDirtyNode(NULL);
	    dnode->lock.release();
	    delete dnode;
	}
    }
#if 0
    // for debugging only
    uval find(ServerObject::DirtySONode *dnode) {
	err_printf("invoking find for dnode %p\n", dnode);
	return dirtySOList.find(dnode);
    }
    // for debugging only (returns 1 if server object is there, 0 otherwise)
    uval find(ServerObject *so) {
	dirtySOList.acquireLock();
	ServerObject::DirtySONode *item;
	void *curr = NULL;
	uval ret = 0;
	while ((curr = dirtySOList.next(curr, item))) {
	    err_printf("Looking at dnode %p, so %p", item, item->so);
	    item->lock.acquire();
	    if (item->locked_isValid()) {
		err_printf("(VALID)\n");
	    } else {
		err_printf("(INVALID)\n");
	    }
	    if (item->locked_isValid() && item->so == so) {
		ret = 1;
		item->lock.release();
		break;
	    } else {
		item->lock.release();
	    }
	}
	dirtySOList.releaseLock();
	return ret;
    }
#endif
};
#endif /* #ifndef __SUPER_BLOCK_H_ */
