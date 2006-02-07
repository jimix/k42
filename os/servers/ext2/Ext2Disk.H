#ifndef __EXT2_DISK_H_
#define __EXT2_DISK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Ext2Disk.H,v 1.9 2004/09/30 03:09:05 apw Exp $
 *****************************************************************************/

#include <io/DiskClientRO.H>
//#include <linux/fs.h>

extern "C" void
k42Bread(unsigned long spBlkSize,
	 uval64 block, int size, char **data, unsigned long *blkSize);

extern "C" void
k42Bwrite(unsigned long blkSize, uval64 block, int size, char *data);

class Ext2Disk {
    // For now assuming we have only one instance of this
    static Ext2Disk *obj;
    DiskClientRef dcr;
    uval blkSize;

public:
    DEFINE_GLOBAL_NEW(Ext2Disk);
    Ext2Disk(DiskClientRef r, uval b) : dcr(r), blkSize(b) {
	//passertMsg(obj == NULL, "Supposed to have only one instance\n");
	obj = this;
    }
    static uval GetBlkSize() {
	passertMsg(obj != NULL, "ops\n");
	return obj->blkSize;
    }
    typedef uval64 sector_t; /* apw */
    static SysStatus Bread(unsigned long spBlkSize,
			   sector_t block, int size, char **data,
			   unsigned long *blkSize);
    static SysStatus BreadPhys(unsigned long pblock, uval paddr) {
	return DREF(obj->dcr)->readBlockPhys(pblock, paddr);
    }
    static SysStatus Bwrite(unsigned long blkSize,
			    sector_t block, int size, char *data);
    static SysStatus BwritePhys(unsigned long pblock, uval paddr) {
	return DREF(obj->dcr)->writeBlockPhys(pblock, paddr);
    }
};
#endif // #ifndef __EXT2_DISK_H_
