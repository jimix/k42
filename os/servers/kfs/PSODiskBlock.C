/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Additions and changes by the K42 group, 2004.
 *
 * $Id: PSODiskBlock.C,v 1.19 2005/04/15 17:39:38 dilma Exp $
 **************************************************************************/

//#include <sys/sysIncs.H>
#include <kfsIncs.H>

#include "PSODiskBlock.H"
#include "KFSGlobals.H"
#include "Disk.H"
#include "BlockCache.H"

PSODiskBlock::PSODiskBlock(KFSGlobals *g, Disk *d):
    PSOBase(g), disk(d)
{
    KFS_DPRINTF(DebugMask::PSO_DISK_BLOCK,
		"PSODiskBlock created for disk 0x%p\n", disk);
}

PSODiskBlock::~PSODiskBlock()
{
}

sval
PSODiskBlock::readBlock(uval32 pblkno, char *buffer,
			PSOBase::AsyncOpInfo *cont /* = NULL */,
			uval isPhysAddr /* = 0 */)
{
    return globals->blkCache->readBlock(pblkno, buffer, cont);
}

sval
PSODiskBlock::writeBlock(uval32 pblkno, char *buffer,
			 PSOBase::AsyncOpInfo *cont /* = NULL */) 
{
    return globals->blkCache->writeBlock(pblkno, buffer, cont);
}

BlockCacheEntry *
PSODiskBlock::readBlockCache(uval32 pblkno)
{
    return  globals->blkCache->getBlockRead(pblkno);
}

void
PSODiskBlock::freeBlockCache(BlockCacheEntry *entry)
{
    globals->blkCache->freeBlock(entry);
}

SysStatus
PSODiskBlock::writeBlockCache(BlockCacheEntry *block, uval32 lblkno)
{
    globals->blkCache->markDirty(block);
    return 0;
}

// free the blocks allocated to this PSO
sval
PSODiskBlock::freeBlocks(uval32, uval32)
{
    return 0;
}

// delete the PSO, and all associated data
void
PSODiskBlock::unlink()
{
}

// flush a dirty PSO to disk
void
PSODiskBlock::flush()
{
}

/*
 * special()
 *
 *   Does nothing.  Must be declared because of virtual tag in class PSOBase.
 */
sval 
PSODiskBlock::special(sval operation, void *buf)
{
    tassertMsg(0, "PSOBasicRWO::special() called\n");
    return -1;
}

// creates a new PSO with the information from the ORSMapEntry
/* virtual */ ServerObject *
PSODiskBlock::clone(ObjTokenID otokID, FSFileKFS *f)
{
    ServerObject *pso = new PSODiskBlock(globals, globals->disk_ar);
    return pso;
}

// allocates disk space for the PSO and returns its location
SysStatusUval
PSODiskBlock::locationAlloc(ObjTokenID _id)
{
    return 0;
}
