#ifndef __DATA_BLOCK_H_
#define __DATA_BLOCK_H_

/***************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataBlock.H,v 1.15 2003/10/27 15:51:04 dilma Exp $
 **************************************************************************/

#include "PSOBase.H"

class KFSGlobals;
class DataBlock {
private:
    KFSGlobals *globals;
    Disk *disk; // the disk to read from
    uval blkno; // the location of this block on disk

    // the buffer holding the data of this block
    char *dataBuf;

public:
    DEFINE_GLOBAL_NEW(DataBlock);
    // constructors
    DataBlock::DataBlock(KFSGlobals *g) : disk(NULL), blkno(0) {
	globals = g;
	dataBuf = (char*) AllocGlobalPadded::alloc(OS_BLOCK_SIZE);
	memset(dataBuf, 0, OS_BLOCK_SIZE);
	tassertMsg(dataBuf != NULL, "no mem");
    }
    DataBlock(uval, uval, KFSGlobals *g);
    DataBlock(Disk *, uval, KFSGlobals *g);

    ~DataBlock() {
	AllocGlobalPadded::free(dataBuf, OS_BLOCK_SIZE);
    }
    // reset the block number
    void setBlkno(uval);

    // retrieve the buffer for this data
    char *getBuf();

    // flush the data block to disk
    sval flush();
};

#endif /* #ifndef __DATA_BLOCK_H_ */
