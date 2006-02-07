#ifndef __PSOSMALL_H_
#define __PSOSMALL_H_
/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: PSOSmall.H,v 1.3 2004/05/05 19:57:59 lbsoares Exp $
 **************************************************************************/

#include "PSOBase.H"
#include "RecordMap.H"

/*
 * A basic physical server object (PSO) for read/write data. This is an
 * imitation of traditional Unix inodes. In particular, in the PSO's record
 * entry (which is currently 124 bytes) we should have 12 direct pointers to
 * data, one pointer to an indirect block, one to a double indirect block and a
 * last to a triple indirect block. This is the exact layout of ext2 except
 * that since ext2 uses 32-bit pointer, each indirect block in ext2 can
 * reference twice as many blocks.
 *
 * Since we have 64-bit pointers (as opposed to 32-bit pointers in "traditional
 * Unix" inodes), we can reference up to ~513 GiB (0.5 TiB) of data using 4KiB
 * blocks (~134 million blocks)
 */

// Max blocks in the record. The count here should be:
//   (KFS_RECORD_SIZE - 3*sizeof(ObjTokenID)) / sizeof(uval32) = (124 - 24) / 4
#define PSO_SMALL_RECORD_MAXBLK 25

// Max block in an indirect block. The count here should be:
//   OS_BLOCK_SIZE / sizeof(uval32) = 4096/4
#define PSO_SMALL_DIRECT_MAXBLK 1024

// log_2(PSO_SMALL_DIRECT_MAXBLK) = 10
#define PSO_SMALL_DIRECT_BITS 10

// maximum number of blocks this PSO can reference
#define PSO_SMALL_MAXBLK (PSO_SMALL_RECORD_MAXBLK + \
			  (1 << PSO_SMALL_DIRECT_BITS) + \
                          (1 << (2 * PSO_SMALL_DIRECT_BITS)) + \
                          (1 << (3 * PSO_SMALL_DIRECT_BITS)) )

class PSOSmall : public PSOBase {
protected:
    PSOBase *llpso;    // lower-level PSO, to be used for IO

    // the PSO record data
    char recordBuf[KFS_RECORD_SIZE];

    // layout of the record data
    struct PSOSmallRecordStruct {
        uval32 dBlk[PSO_SMALL_RECORD_MAXBLK]; // direct pointers to data
    private:
	uval32 indirectBlkno;           // pointer to indirect block
	uval32 doubleIndirectBlkno;     // pointer to double indirect block
	uval32 tripleIndirectBlkno;     // pointer to triple indirect block

    public:
	uval32 getIndirectBlkno() { return TE32_TO_CPU(indirectBlkno); }
	uval32 getDoubleIndirectBlkno() {
	    return TE32_TO_CPU(doubleIndirectBlkno);
	}
	uval32 getTripleIndirectBlkno() {
	    return TE32_TO_CPU(tripleIndirectBlkno);
	}
	void setIndirectBlkno(uval32 v) {
	    indirectBlkno = CPU_TO_TE32(v);
	}
	void setDoubleIndirectBlkno(uval32 v) {
	    doubleIndirectBlkno = CPU_TO_TE32(v);
	}
	void setTripleIndirectBlkno(uval32 v) {
	    tripleIndirectBlkno = CPU_TO_TE32(v);
	}
    };

    // This should point to recordBuf, but as a struct is easier to manipulate
    // than a "raw" record
    PSOSmallRecordStruct *record;

    // functions which assume that the PSO's lock is already held.
    virtual void locked_flush();
    virtual sval locked_freeBlocks(uval32 fromBlk, uval32 toBlk);
    virtual void locked_unlink();

    virtual SysStatusUval blockToPath(uval32 lblkno, uval32 offsets[4]);
    virtual uval32 getDblk(uval32, uval8);
    virtual void freeDblk(uval32 lblkno);
    virtual void truncate(uval32 lblkno);
public:
    DEFINE_GLOBAL_NEW(PSOSmall);

    // constructors
    PSOSmall(PSOBase *p, KFSGlobals *g) : PSOBase(g), llpso(p) {
	memset(recordBuf, 0, KFS_RECORD_SIZE);
	record = NULL;
    }

    PSOSmall(ObjTokenID *otokID, RecordMapBase *r, PSOBase *p, KFSGlobals *g);

    virtual ~PSOSmall();

    // handle the reading and writing of data
    virtual sval readBlock(uval32 lblkno, char *buffer, uval local,
			   uval isPhysAddr = 0);
    virtual sval writeBlock(uval32, char *, uval);
    virtual sval readBlockPhys(uval32, char *, uval);

    // block-cache integration stuff
    virtual BlockCacheEntry *readBlockCache(uval32 b, uval local);
    virtual void freeBlockCache(BlockCacheEntry *entry) {
	llpso->freeBlockCache(entry);
    }

    virtual SysStatus writeBlockCache(BlockCacheEntry *block, uval32 lblkno);

    // free the blocks allocated to this PSO
    virtual sval freeBlocks(uval32 fromBlk, uval32 toBlk);

    // delete the PSO, and all associated data
    virtual void unlink();

    // flush a dirty PSO to disk
    virtual void flush();

    // extra functionality for user-created PSOs
    virtual sval special(sval, void *);

    // creates a new PSO with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID *otokID, RecordMapBase *r);

    // allocates disk space for the PSO and returns its location
    virtual SysStatusUval locationAlloc(ObjTokenID *otokID,
					RecordMapBase *recordMap);
};

#endif /* #ifndef __PSOSMALL_H_ */
