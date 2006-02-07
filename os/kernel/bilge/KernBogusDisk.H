#ifndef __KERN_BOGUS_DISK_H_
#define __KERN_BOGUS_DISK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernBogusDisk.H,v 1.1 2003/11/08 17:29:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Bogus disk interface
 * **************************************************************************/

#include "KernSimDisk.H"

class KernBogusDisk : public KernSimDisk {
private:
    uval diskID;
    BLock pageBufLock;
    uval pageBufAddr;

protected:
    void init();

public:
    static SysStatus Create(KernBogusDisk* &obj, uval simosDisk);
    DEFINE_GLOBAL_NEW(KernBogusDisk);
    KernBogusDisk(uval id) { diskID = id; }
    ~KernBogusDisk() {}

__xpublic:
    virtual SysStatus _simDiskValid();

    virtual SysStatusUval _getBlockSize();

    virtual SysStatusUval _getDevSize();

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
#endif /* #ifndef __KERN_BOGUS_DISK_H_ */
