#ifndef __DISK_H_
#define __DISK_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Disk.H,v 1.16 2004/09/15 20:45:04 dilma Exp $
 *****************************************************************************/

#include "PSOBase.H"

// class that defines the disk interface
class Disk
{
public:
    virtual ~Disk() { }

    // Find capacity of disk
    virtual SysStatus readCapacity(uval &nBlocks, uval &bSize) = 0;

    // Asynchronous block operations.
    virtual SysStatus aReadBlock(uval blkno, char *buf,
				 PSOBase::AsyncOpInfo *cont) = 0;
    virtual SysStatus aWriteBlock(uval blkno, char *buf,
				  PSOBase::AsyncOpInfo *cont) = 0;

    // Synchronous block operations.
    virtual SysStatus readBlock(uval blkno, char *buf) = 0;
    virtual SysStatus writeBlock(uval blkno, char *buf) = 0;
};

#endif // __DISK_H_
