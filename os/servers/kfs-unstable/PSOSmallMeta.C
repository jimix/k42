/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: PSOSmallMeta.C,v 1.2 2004/05/05 19:57:59 lbsoares Exp $
 **************************************************************************/

#include <kfsIncs.H>

#include "PSOSmallMeta.H"

/*
 * getDblk()
 *
 *   Given a certain logical block number, returns the physical block number
 *   it's mapped to.
 *   The 'create' flag determines if new blocks should be allocated or just 
 *   return 0 in the case the mapping is previously not present.
 */
uval32
PSOSmallMeta::getDblk(uval32 lblkno, uval8 create)
{
    uval32 blkno;

    // first, try the hash
    KFSHashEntry<uval32> *entry;

    if (blockMap.findAddLock(lblkno, &entry)) {
	blkno = entry->getData();
	// if creating, remove current mapping if it's null
	if (!create || blkno) {
	    entry->unlock();
	    return blkno;
	}
    }

//    err_printf("not found: l=%llu, d=%llu, c=%u\n", lblkno, blkno, create);

    blkno = PSOSmall::getDblk(lblkno, create);

    // insert it in the hash for later
    entry->setData(blkno);
    entry->unlock();

    return blkno;
}

/*
 * freeDblk()
 * 
 *    Given a certain logical number, frees the block, zeroes-out its entry
 *    in the correct meta-data block, and possibly frees unused meta-data 
 *    block.
 */
void
PSOSmallMeta::freeDblk(uval32 lblkno)
{
    KFSHashEntry<uval32> *entry;
    uval found = blockMap.findAddLock(lblkno, &entry);
    tassertMsg(found == 1, "buggy\n");
    entry->setData(0);
    entry->unlock();
    
//    err_printf("free: l=%llu\n", lblkno);

    PSOSmall::freeDblk(lblkno);
    
    return;
}

/*
 * truncate()
 * 
 *    Given a certain logical number, deletes all blocks greater than
 *    or equal to it. It frees all blocks, zeroing out entries in the
 *    meta-data blocks, and possibly freeing unused meta-data blocks
 */
void
PSOSmallMeta::truncate(uval32 lblkno)
{
    // easiest thing for now is to invalid the whole hash
    blockMap.removeAll();

//    err_printf("truncate: l=%llu\n", lblkno);
    PSOSmall::truncate(lblkno);

    return;
}

/*
 * clone()
 *
 *   Creates a new PSOSmallMeta from the given "object related state" map entry
 */
ServerObject *
PSOSmallMeta::clone(ObjTokenID *otokID, RecordMapBase *r)
{
    ServerObject *pso = new PSOSmallMeta(otokID, r, llpso, globals);
    return pso;
}
