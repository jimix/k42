#ifndef __KERN_TOY_DISK_H_
#define __KERN_TOY_DISK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernToyDisk.H,v 1.10 2003/11/08 17:29:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Disk interface for simos sim disk
 * **************************************************************************/

#include "KernSimDisk.H"

class KernToyDisk : public KernSimDisk {
private:
    uval simosDiskID;
    BLock pageBufLock;
    uval pageBufAddr;

protected:
    virtual void init();

public:
    static void ClassInit();
    static SysStatus Create(KernToyDisk* &obj, uval simosDisk);
    DEFINE_GLOBAL_NEW(KernToyDisk);
    KernToyDisk(uval simosDisk) {
	simosDiskID = simosDisk;
    }
    ~KernToyDisk() {}

__xpublic:
    virtual SysStatus _simDiskValid();
    virtual SysStatusUval _getBlockSize() {
	// FIXME: avoid these hardcoded values
	return _SRETUVAL(4096);
    }
    virtual SysStatusUval _getDevSize() {
	// FIXME: avoid these hardcoded values
	return _SRETUVAL(262144*4096);
    }


    // operations that write from PPC page for virtual memory
    virtual SysStatusUval _writeVirtual(__in uval offset,
					__inbuf(length) const char* buf,
					__in uval length);

    virtual SysStatusUval _readVirtual(__in uval offset,
				       __outbuf(__rc:buflength) char* buf,
				       __in uval buflength);

    // operations that deal physical addresses
    virtual SysStatusUval _writePhys(__in uval offset, __in uval paddr);
    virtual SysStatusUval _readPhys(__in uval offset, __in uval paddr);
};
#endif /* #ifndef __KERN_TOY_DISK_H_ */
