#ifndef __KFS_DISK_H_
#define __KFS_DISK_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: KFSDisk.H,v 1.2 2004/02/21 21:30:18 dilma Exp $
 *****************************************************************************/

#include <io/DiskClient.H>
#include "Disk.H"

// class used by the KFS server
class KFSDisk : public Disk {
private:
    DiskClientRef dcr;

public:
    DEFINE_GLOBAL_NEW(KFSDisk);
    KFSDisk::KFSDisk(){}
    virtual ~KFSDisk(){
	// FIXME: the following doesn't seem correct, should
	// be destroy() ? Taking out for now
	//delete *(DiskClient **)dcr;
    }
    //  Constructor will mount the pseudo-disk
    SysStatus init(char *, uval);

    // Find capacity of disk
    SysStatus readCapacity(uval &nBlocks, uval &bSize);

    // Asynchronous block operations.
    SysStatus aReadBlock(uval, char *);
    SysStatus aWriteBlock(uval, char *);

    // Synchronous block operations.
    SysStatus readBlock(uval, char *);
    SysStatus writeBlock(uval, char *);
};

#endif // __KFS_DISK_H_
