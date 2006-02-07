#ifndef __TOY_BLOCK_DEV_H_
#define __TOY_BLOCK_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ToyBlockDev.H,v 1.14 2004/08/12 20:46:37 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Block Device Interface for simulated disks
 * **************************************************************************/

#include "BlockDevBase.H"
#include "KernSimDisk.H"
#include <io/MemTrans.H>

class ToyBlockDev;
typedef ToyBlockDev** ToyBlockDevRef;
class ToyBlockDev : public BlockDevBase {
    KernSimDisk *kdisk;
    uval isValid;
public:
    DEFINE_GLOBALPADDED_NEW(ToyBlockDev);
    DEFINE_ACCESS_SERVER(FileLinuxServer,
			 MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			 MetaObj::none);

    DEFINE_REFS(ToyBlockDev);

    virtual SysStatus devOpen() { return 0; };

    virtual SysStatus putBlock(uval physAddr, uval len, uval objOffset);
    virtual SysStatus getBlock(uval physAddr, uval len, uval objOffset);

__xpublic:

    // FIXME: get rid of these, can't have synchronous calls from kernel
    //        to server
    virtual SysStatus _write(__in uval virtAddr, __in uval objOffset,
			     __in uval len, __XHANDLE xhandle);
    // BlockDev interface

    virtual SysStatus _getBlock(__in uval addr, __in uval size,
				__in uval objOffset,__XHANDLE xhandle);

    virtual SysStatus init(const char *name, int simosDisk);

    static void ClassInit();

__xpublic:

    virtual SysStatusUval _ioctl(__in uval req,
				 __inoutbuf(size:size:size) char* buf,
				 __inout uval &size);

    virtual SysStatus destroy() {
	err_printf("called %s\n",__func__);
	return 0;
    }

};
#endif /* #ifndef __TOY_BLOCK_DEV_H_ */

