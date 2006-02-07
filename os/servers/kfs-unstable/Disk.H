#ifndef __DISK_H_
#define __DISK_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Disk.H,v 1.2 2004/02/21 21:30:17 dilma Exp $
 *****************************************************************************/

// class that defines the disk interface
class Disk
{
public:
    virtual ~Disk() { }

    // Find capacity of disk
    virtual SysStatus readCapacity(uval &nBlocks, uval &bSize) = 0;

    // Asynchronous block operations.
    virtual SysStatus aReadBlock(uval, char *) = 0;
    virtual SysStatus aWriteBlock(uval, char *) = 0;

    // Synchronous block operations.
    virtual SysStatus readBlock(uval, char *) = 0;
    virtual SysStatus writeBlock(uval, char *) = 0;
};

#endif // __DISK_H_
