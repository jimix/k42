#ifndef __KFS_DISK_H_
#define __KFS_DISK_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: KFSDisk.H,v 1.10 2004/09/15 20:46:00 dilma Exp $
 *****************************************************************************/

#include <io/DiskClient.H>
#include <io/DiskClientAsync.H>
#include "Disk.H"

// class used by the KFS server
class KFSDisk : public Disk {
private:
    DiskClientRef dcr;
    DiskClientAsyncRef adcr;

    static void CallBack(uval rc, uval args);

public:
    DEFINE_GLOBAL_NEW(KFSDisk);
    KFSDisk::KFSDisk(){}
    virtual ~KFSDisk(){
	// FIXME: the following doesn't seem correct, should
	// be destroy() ? Taking out for now
	//delete *(DiskClient **)dcr;
    }
    //  Constructor will mount the pseudo-disk
    SysStatus init(char *dname, uval readOnly);

    // Find capacity of disk
    SysStatus readCapacity(uval &nBlocks, uval &bSize);

    // Asynchronous block operations.
    SysStatus aReadBlock(uval blkno, char *buf, PSOBase::AsyncOpInfo *cont);
    SysStatus aWriteBlock(uval blkno, char *buf, PSOBase::AsyncOpInfo *cont);

    // Synchronous block operations.
    SysStatus readBlock(uval blkno, char *buf);
    SysStatus writeBlock(uval blkno, char *buf);
};

#endif // __KFS_DISK_H_
