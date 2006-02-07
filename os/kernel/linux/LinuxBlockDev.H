#ifndef __LINUX_BLOCK_DEV_H_
#define __LINUX_BLOCK_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxBlockDev.H,v 1.2 2004/08/12 20:46:37 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Block Device Interface for Linux block devices
 * **************************************************************************/
#include "../bilge/BlockDevBase.H"

class LinuxBlockDev;
typedef LinuxBlockDev** LinuxBlockDevRef;
class LinuxBlockDev : public BlockDevBase {
protected:

    void* devData;  // data from the device from open() call later

public:
    DEFINE_GLOBALPADDED_NEW(LinuxBlockDev);
    DEFINE_ACCESS_SERVER(FileLinuxServer,
			 MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			 MetaObj::none);

    DEFINE_REFS(LinuxBlockDev);

    virtual SysStatus init(int deviceID,const char *name,
			   ObjectHandle dir, ObjectHandle &node);

    static void ClassInit(VPNum vp);

    virtual SysStatus devOpen();
    virtual SysStatus putBlock(uval physAddr, uval len, uval objOffset);
    virtual SysStatus getBlock(uval physAddr, uval len, uval objOffset);

    virtual SysStatus asyncOp(uval type, uval physAddr, uval len,
			      uval objOffset, IOCompletion *ioc);
    virtual SysStatus ioComplete(struct IOCmdState* is, SysStatus err);
__xpublic:
    virtual SysStatus _write(__in uval virtAddr, __in uval objOffset,
			     __in uval len, __XHANDLE xhandle);
    // BlockDev interface

    virtual SysStatus _getBlock(__in uval srcAddr, __in uval size,
				__in uval objOffset,__XHANDLE xhandle);

__xpublic:

    virtual SysStatusUval _ioctl(__in uval req,
				 __inoutbuf(size:size:size) char* buf,
				 __inout uval &size);

};
#endif /* #ifndef __LINUX_BLOCK_DEV_H_ */

