#ifndef __KERN_SIM_DISK_H_
#define __KERN_SIM_DISK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernSimDisk.H,v 1.1 2003/11/08 17:29:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: abstract interface for simulated disks
 * **************************************************************************/

class KernSimDisk : public Obj {
protected:
    virtual void init() = 0;

__xpublic:
    virtual SysStatus _simDiskValid() = 0;
    virtual SysStatusUval _getBlockSize() = 0;
    virtual SysStatusUval _getDevSize() = 0;

    // operations that write from PPC page for virtual memory
    virtual SysStatusUval _writeVirtual(__in uval offset,
					__inbuf(length) const char* buf,
					__in uval length) = 0;

    virtual SysStatusUval _readVirtual(__in uval offset,
				       __outbuf(__rc:buflength) char* buf,
				       __in uval buflength) = 0;

    // operations that deal physical addresses
    virtual SysStatusUval _writePhys(__in uval offset, __in uval paddr) = 0;
    virtual SysStatusUval _readPhys(__in uval offset, __in uval paddr) = 0;
};
#endif /* #ifndef __KERN_SIM_DISK_H_ */
