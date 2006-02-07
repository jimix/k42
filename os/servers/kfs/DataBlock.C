/***************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataBlock.C,v 1.18 2003/10/27 15:51:04 dilma Exp $
 **************************************************************************/

#include "kfsIncs.H"
#include "KFSDebug.H"
#include "Disk.H"
#include "SuperBlock.H"
#include "DataBlock.H"
#include "KFSGlobals.H"

#ifdef KFS_TOOLS
#include <stdio.h>
#endif

/*
 * DataBlock()
 *
 *   Reads the block specified by the given id.
 */
DataBlock::DataBlock(uval part, uval block, KFSGlobals *g)
    : disk(g->disk_ar), blkno(block)
{
    globals = g;
    dataBuf = (char *) AllocGlobalPadded::alloc(OS_BLOCK_SIZE);
    tassertMsg(dataBuf != NULL, "no mem");

    memset(dataBuf, 0, OS_BLOCK_SIZE);

    // read in the block if there is a block to read...
    KFS_DPRINTF(DebugMask::DATA_BLOCK,
		"DataBlock with part created for block number %lu\n", blkno);

    if(blkno && disk) {
        SysStatus rc = disk->readBlock(blkno, dataBuf);
	// FIXME dilma : we should deal with this error in a nice way,
	//               but we're in a constructor ...
	passertMsg(_SUCCESS(rc), "disk block read failed\n");
    }
}

DataBlock::DataBlock(Disk *d, uval block, KFSGlobals *g)
    : disk(d), blkno(block)
{
    globals = g;

    dataBuf = (char*) AllocGlobalPadded::alloc(OS_BLOCK_SIZE);
    tassertMsg(dataBuf != NULL, "no mem");

    memset(dataBuf, 0, OS_BLOCK_SIZE);

    // read in the block if there is a block to read...
    KFS_DPRINTF(DebugMask::DATA_BLOCK,
		"DataBlock with disk created for block number %lu\n", blkno);

    if(blkno && disk) {
        SysStatus rc = disk->readBlock(blkno, dataBuf);
	// FIXME dilma : we should deal with this error in a nice way,
	//               but we're in a constructor ...
	passertMsg(_SUCCESS(rc), "disk block read failed\n");
    }
}

/*
 * setBlkno()
 *
 *   Resets the block number of the data block.  This is useful to
 *   write a block to a new location, or to create a fresh data block
 *   with all new data and write that to the disk without reading.
 */
void
DataBlock::setBlkno(uval b)
{
    blkno = b;
}

/*
 * getBuf()
 *
 *   Retrieve's the data buffer and returns it to the caller.  If the
 *   buffer is not in memory, it is read off the disk.
 */
char *
DataBlock::getBuf()
{
    return dataBuf;
}

/*
 * flush()
 *
 *   Sync the data to disk.
 */
sval
DataBlock::flush()
{
    // write out the block
    if(blkno) {
        return disk->writeBlock(blkno, dataBuf);
    }

    return 0;
}
