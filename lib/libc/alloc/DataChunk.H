#ifndef __DATA_CHUNK_H_
#define __DATA_CHUNK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataChunk.H,v 1.5 2001/10/05 21:47:07 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Every block memory managed by the alloc system is treated as a
 * DataChunk when being manipulated internally (i.e. when it contains no
 * valid data and is linked through various lists).  Therefore, every
 * block must be at least as big as a DataChunk.
 *
 * A DataChunk has two pointers.  One ("next") points to the next
 * DataChunk in a list of DataChunks, and the other ("nextlist") points
 * to the next list in a list of DataChunks.  Each components of the alloc
 * subsystem is free to use these pointers as they see fit.
 * **************************************************************************/

class DataChunk {
public:
    DataChunk *next;
    DataChunk *nextList;
    void print();
    uval getNumBlocks();
};

#endif /* #ifndef __DATA_CHUNK_H_ */
