#ifndef __PSO_DISK_BLOCK_H_
#define __PSO_DISK_BLOCK_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: PSODiskBlock.H,v 1.3 2004/05/05 19:57:59 lbsoares Exp $
 **************************************************************************/

#include "PSOBase.H"

#define PSO_DISKBLOCK_NONE  0x00
#define PSO_DISKBLOCK_DIRTY 0x01

class Disk;

/*
 * A "low-level" type Physical Server Object (PSO) for communicating
 * with real disk blocks. This PSO is to be used by other PSOs,
 * ORSMap, and SuperBlock objects to communicate with disk.
 *
 * This object is pretty much stateless and should be shared between
 * of objects trying to do I/O operations on a "disk".
 */
class PSODiskBlock : public PSOBase {
private:
    Disk *disk; // the disk to read from

public:
    DEFINE_GLOBAL_NEW(PSODiskBlock);

    // constructors
    PSODiskBlock(KFSGlobals *, Disk *);

    ~PSODiskBlock();

    // handle the reading and writing of data
    sval readBlock(uval32 pblkno, char *buffer, uval local,
		   uval isPhysAddr = 0);
    sval writeBlock(uval32, char *, uval);
    BlockCacheEntry *readBlockCache(uval32, uval);
    void freeBlockCache(BlockCacheEntry *);
    SysStatus writeBlockCache(BlockCacheEntry *block, uval32 lblkno);

    // free the blocks allocated to this PSO
    sval freeBlocks(uval32, uval32);

    // delete the PSO, and all associated data
    void unlink();

    // flush a dirty PSO to disk
    void flush();

    // mark the PSO as dirty
    void markDirty(uval32) { }
    SysStatusUval isDirty() { return 0; }

    // extra functionality for user-created PSOs
    sval special(sval, void *);

    // creates a new PSO with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID *otokID, RecordMapBase *r);

    // allocates disk space for the PSO and returns its location
    SysStatusUval locationAlloc(ObjTokenID *otokID, RecordMapBase *recordMap);
};

#endif /* #ifndef __DATA_BLOCK_H_ */
