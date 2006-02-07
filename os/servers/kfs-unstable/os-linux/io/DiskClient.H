#ifndef __DISK_CLIENT_H_
#define __DISK_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClient.H,v 1.1 2004/02/11 23:04:00 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to Disk Objects
 * **************************************************************************/
#include <cobj/Obj.H>
class DiskClient;
typedef DiskClient **DiskClientRef;

class DiskClient:public Obj
{
    /*
    StubBlockDev sbd;
    StubBlockDev sbdPhys;
    XHandle partner;
    */
    kdev_t dev;
    uval size;
public:
    DEFINE_GLOBAL_NEW(DiskClient);
    DEFINE_REFS(DiskClient);
    DiskClient();
    virtual ~DiskClient() { }
    static const uval BLOCKSIZE = 4096;
    virtual SysStatusUval getSize() { return size; };
    virtual SysStatus readBlock(uval blockNumber, void *block);
    virtual SysStatus writeBlock(uval blockNumber, void *block);

    virtual SysStatus readBlockPhys(uval blockNumber, uval paddr);
    virtual SysStatus writeBlockPhys(uval blockNumber, uval paddr);

    static void ClassInit(VPNum vp);
    static SysStatus Create(DiskClientRef &dcr, char *blockDev);
#ifndef TOOLSIDE
    SysStatus init(char *blockDev);
    static uval Init(char *blockDev);
#else /* #ifndef TOOLSIDE */
    // non-zero result means new disk created, requires format
    static uval Init(char *diskFile);
#endif /* #ifndef TOOLSIDE */
};
#endif /* #ifndef __DISK_CLIENT_H_ */
