/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Ext2Disk.C,v 1.6 2004/09/30 03:08:14 apw Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "Ext2Disk.H"

/* static */ Ext2Disk* Ext2Disk::obj = NULL;


extern "C" void
k42Bread(unsigned long spBlkSize, uval64 block, int size, char **data,
	 unsigned long *blkSize)
{
    SysStatus rc = Ext2Disk::Bread(spBlkSize, block, size, data, blkSize);
    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);
}

extern "C" void
k42Bwrite(unsigned long blkSize, uval64 block, int size, char *data)
{
    SysStatus rc = Ext2Disk::Bwrite(blkSize, block, size, data);
    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);
}

/* static */ SysStatus
Ext2Disk::Bread(unsigned long spBlkSize,
		uval64 block, int size, char **data, unsigned long *blkSize)
{
    // This is so ugly ... we're allocating the buffer for the data,
    // never releasing. The goal is to see if we're able to read
    // properly a couple of blocks.
    // We are not incrementing the reference count

    passertMsg((unsigned long) size <= spBlkSize,
	       "size is %d, spBlkSize is %ld\n",
	       size, spBlkSize);

    // Allocating a lot of stuff, ugly ugly, but we need to make sure
    // we read something
    char *buf = (char*) AllocGlobalPadded::alloc(obj->blkSize); 
    *data = (char*) AllocGlobalPadded::alloc(size); 

    // compute appropriate physical block number
    uval64 pblock = block * spBlkSize / obj->blkSize;
    uval offset = (block * spBlkSize) % obj->blkSize;

    SysStatus rc = DREF(obj->dcr)->readBlock(pblock, buf); 
    _IF_FAILURE_RET(rc);
    // copy into data
    memcpy(*data, buf + offset, size);
  
    *blkSize = obj->blkSize;

    return 0;
}

/* static */ SysStatus
Ext2Disk::Bwrite(unsigned long blkSize,
		 uval64 block, int size, char *data)
{
    passertMsg((unsigned long) size <= blkSize,
	       "size is %d, blkSize is %ld\n",
	       size, blkSize);

    // compute appropriate physical block number
    uval64 pblock = block * blkSize / obj->blkSize;

    SysStatus rc = DREF(obj->dcr)->writeBlock(pblock, data); 
    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);
  
    return rc;
}
