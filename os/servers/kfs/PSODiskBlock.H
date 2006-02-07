#ifndef __PSO_DISK_BLOCK_H_
#define __PSO_DISK_BLOCK_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Additions and changes by the K42 group, 2004.
 *
 * $Id: PSODiskBlock.H,v 1.12 2004/09/15 20:47:38 dilma Exp $
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
    sval readBlock(uval32 pblkno, char *buffer,
		   PSOBase::AsyncOpInfo *cont = NULL,
		   uval isPhysAddr = 0);
    sval writeBlock(uval32 b, char *buffer,
		    PSOBase::AsyncOpInfo *cont = NULL);
    BlockCacheEntry *readBlockCache(uval32 lblk);
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
    virtual ServerObject *clone(ObjTokenID, FSFileKFS *f);

    // allocates disk space for the PSO and returns its location
    SysStatusUval locationAlloc(ObjTokenID);
};

#endif /* #ifndef __DATA_BLOCK_H_ */
