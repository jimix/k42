#ifndef __FILE_DISK_H_
#define __FILE_DISK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileDisk.H,v 1.7 2004/09/15 20:49:45 dilma Exp $
 *****************************************************************************/
#include "fs.H"
#include "Disk.H"

#include <fcntl.h>


class FileDisk : public Disk {
public:
    static const uval DEFAULT_NUMBER_BLOCKS = 262144;
private:
    uval numBlocks, blockSize;

    struct flock fileLock;
    int fd;

public:
    FileDisk(int filefd, uval nBlocks, uval bSize);
    FileDisk(int filefd);
    virtual ~FileDisk();

    // Find capacity of disk
    virtual SysStatus readCapacity(uval &nBlocks, uval &bSize);

    // Asynchronous block operations.
    virtual SysStatus aReadBlock(uval blkno, char *buffer, 
				 PSOBase::AsyncOpInfo *cont);
    virtual SysStatus aWriteBlock(uval blkno, char *buffer,
				 PSOBase::AsyncOpInfo *cont);

    // Synchronous block operations.
    virtual SysStatus readBlock(uval, char *);
    virtual SysStatus writeBlock(uval, char *);
};

#endif /* #ifndef __FILE_DISK_H_ */
