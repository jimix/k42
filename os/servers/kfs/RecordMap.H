#ifndef __RECORD_MAP_H_
#define __RECORD_MAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Many corrections/changes by Livio Soares (livio@ime.usp.br)
 *
 * $Id: RecordMap.H,v 1.33 2004/05/06 21:05:55 dilma Exp $
 *****************************************************************************/

#include "ObjToken.H"
#include "PSOBase.H"
#include "KFSDebug.H"
#include "BlockCache.H"
#include "PSOTypes.H"
#include "Byteorder.H"

#define RMAP_RECORD_SIZE           (128)
// reserve 4 bytes for the PsoType
#define KFS_RECORD_SIZE            (124)


// The RecordMapBlock is divided into:
//   psoRecord[RMAP_RECORD_SIZE] - record for underlying PSO
//   bitmap[4096-RMAP_RECORD_SIZE] - free-block bitmap mapping

// Max. number of blocks used for the bitmap
#define RMAP_BITMAP_BLOCKS   ((4096-RMAP_RECORD_SIZE)/sizeof(uval32))

// Number of 32-bit words in a page
#define RMAP_WORDS_PAGE      (4096/sizeof(uval32))
// Number of records (bits) per page
#define RMAP_BITS_PAGE       (4096*8)

#define RMAP_NONE  0x00
#define RMAP_DIRTY 0x01

/*
 * The RecordMap class was a PSO used to actually store the data of
 * fixed sized record PSOs (PSONmRec).  It is kept as an array of
 * PSO_RMAP_RECORD_SIZE records which can be allocated from an
 * internally stored and managed bitmap.
 */
class RecordMap {
protected:
    KFSGlobals *globals;
    BLock lock;
    PSOBase *pso;          // underlying PSO, used to actually keep the data
    uval32 flags;
    uval32 blkno;

    // the actual metadata storage
    BlockCacheEntry *dataBlock;

    // This structure should occupy one (4k) block 
    struct RecordMapStruct {
    private:
	// Record for the underlying PSO, which actually keeps the data
	char psoRecord[RMAP_RECORD_SIZE];
	// mapping for the blocks which contain the free-record bitmap
	uval32 bitmap[RMAP_BITMAP_BLOCKS] __attribute__ ((packed));

    public:
	char *getPsoRecordPtr() { return psoRecord; }

	uval32 getPsoRecordType(int index) {
	    char *rec = psoRecord + index *  RMAP_RECORD_SIZE;
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("getPSORecordType for index %d (psoRecord %p, rec %p) "
		       "is returning 0x%x\n", index, psoRecord, rec,
		       TE32_TO_CPU(((uval32*)rec)[0]));
#endif // #ifdef KFS_DEBUG_ENDIAN
	    return TE32_TO_CPU(((uval32*)rec)[0]);
	}
	void setPsoRecordType(PsoType type, int index) {
	    char *rec = psoRecord + index *  RMAP_RECORD_SIZE;
	    uval32 t = CPU_TO_TE32((uval32) type);
	    memcpy((void*) rec, &t, sizeof(uval32));
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("In setPsoRecordType with type %ld, index %d, "
		       "psoRecord addr %p, addr for index %p\n",
		       (uval) type, index, psoRecord, rec);
	    err_printf("calling getPSORecordPtr to see what ended up there\n");
	    (void) getPsoRecordType(index);
#endif // #ifdef KFS_DEBUG_ENDIAN
	}
	uval32 getBitmap(int i) {
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("In getBitmap(%d) addr is %p, CPU value 0x%x ",
		       i, &bitmap[i], TE32_TO_CPU(bitmap[i]));
	    PRINT_TE32(bitmap[i], 1);
#endif // #ifdef KFS_DEBUG_ENDIAN
	    return TE32_TO_CPU(bitmap[i]);
	}
	void setBitmap(int i, uval32 v) { bitmap[i] = CPU_TO_TE32(v); }
    };

    struct RecordMapStruct *data;

    // Internal version of {alloc,free}Record
    virtual SysStatus internal_allocRecord(PsoType, ObjTokenID *);
    virtual SysStatus internal_freeRecord(ObjTokenID *);

    // functions which assume that the PSO's lock is already held.
    virtual uval32 locked_flush();
    virtual void locked_markDirty();

public:
    DEFINE_GLOBAL_NEW(RecordMap);

    RecordMap(uval32 blkno, KFSGlobals *g);
    virtual ~RecordMap();

    virtual void init();

    // returns an offset to a sequential run of 64-byte records
    virtual SysStatus allocRecord(PsoType, ObjTokenID *);
    virtual sval freeRecord(ObjTokenID *);

    // get a record of the given size at the given location
    virtual sval getRecord(ObjTokenID, char *);
    virtual sval setRecord(ObjTokenID, char *);

    virtual sval setRecordType(ObjTokenID, PsoType);
    virtual PsoType getRecordType(ObjTokenID);

    // flush a dirty PSO to disk
    virtual uval32 flush();

    // mark the PSO as dirty
    virtual void markDirty();

    virtual uval isDirty() { return flags |= RMAP_DIRTY; }

    // creates a new PSO with the information from the ORSMapEntry
    static SysStatusUval locationAlloc(KFSGlobals *globals);

    virtual SysStatus sweepBitMap();
};

#endif /* #ifndef __RECORD_MAP_H_ */
