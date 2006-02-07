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
 * $Id: KFSDisk.C,v 1.3 2004/11/23 22:45:52 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>
#include <io/DiskClientRO.H>

#include "KFSDisk.H"
#include "SuperBlock.H"

/*
 * Initializer will mount the pseudo-disk
 * ======================================
 */
SysStatus
KFSDisk::init(char *dname, uval readOnly)
{
    SysStatus rc;

    if (readOnly) {
	rc = DiskClientRO::Create(dcr, dname);
    }
    else {
	rc = DiskClient::Create(dcr, dname);
    }
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);
    return rc;
}

/*
 * Find capacity of disk
 * 
 */
SysStatus
KFSDisk::readCapacity(uval &lblocks, uval &lblocklen)
{
    SysStatusUval rc;
    rc = DREF(dcr)->getSize();
    if (_SUCCESS(rc)) {
	lblocks = _SGETUVAL(rc) / OS_BLOCK_SIZE;
	//err_printf("readCapacity returning lblocks %ld\n", lblocks);
    } else {
	tassertWrn(0, "KFSDisk::readCapacity returning error rc 0x%lx\n", rc);
	return rc;
    }
    lblocklen = OS_BLOCK_SIZE;

    return 0;
}

/*
 * Do an asynchronous read block operation.
 * Copy data into location specified by paddr.
 * ===========================================
 */
SysStatus
KFSDisk::aReadBlock(uval blkno, char *buf)
{
    return DREF(dcr)->readBlockPhys(blkno, (uval)buf);
}

/*
 * Do an asynchronous write block operation.
 * Copy data from location specified by 'buf'.
 * ===========================================
 */
SysStatus
KFSDisk::aWriteBlock(uval blockno, char *buf)
{
    return DREF(dcr)->writeBlockPhys(blockno, (uval)buf);
}

/*
 * Read block # 'blkno' & copy data into location specified by 'buf'
 * Synchronous read.
 * ==================================================================
 */
SysStatus
KFSDisk::readBlock(uval blkno, char *buf)
{
    //err_printf("In KFSDisk::readBlock for block=%lu, obj=0x%p, dcr=0x%p\n", blkno, this, dcr);
    return DREF(dcr)->readBlock(blkno, buf);
}

/*
 * Write to block # blockno from data in location specified by 'buf'
 * Synchronous write.
 * ==================================================================
 */
SysStatus
KFSDisk::writeBlock(uval blkno, char *buf)
{
    //err_printf("In KFSDisk::writeBlock for obj %p\n", this);
    return DREF(dcr)->writeBlock(blkno, buf);
}
