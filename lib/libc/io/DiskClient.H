#ifndef __DISK_CLIENT_H_
#define __DISK_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClient.H,v 1.17 2005/06/02 20:05:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to Disk Objects
 * **************************************************************************/
#include <stub/StubBlockDev.H>
#include <io/MemTrans.H>
#include <cobj/Obj.H>
#include <io/CallBackObj.H>
#include <misc/SharedBufferConsumerUser.H>

#ifdef _IN_STUBGEN
// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#else
#include <meta/MetaCallBackObj.H>
#endif /* #ifndef _IN_STUBGEN */

class DiskClient;
typedef DiskClient **DiskClientRef;

class DiskClient : public CallBackObj {
protected:
    StubBlockDev sbd;
    StubBlockDev sbdPhys;
    uval diskSize;
    uval blkSize;
    uval numBlocks;
    SysStatus syncRequest(StubBlockDev &stub,
			  uval type, uval addr, uval size, uval offset);

    /* The new class inherits from Obj, but it won't be used as a
     * clustered object */
    class DiskTransportConsumer : public SharedBufferConsumerUser {
    public:
	DEFINE_GLOBAL_NEW(DiskTransportConsumer);
	DiskTransportConsumer() {}
	virtual SysStatus init(ObjectHandle transportFROH,
			       ProcessID pidProducer);
	virtual SysStatus getRequest(uval &token, uval &err);
    };
    DiskTransportConsumer *transportPhys;
    DiskTransportConsumer *transportVirt;

public:
    DEFINE_GLOBAL_NEW(DiskClient);
    DEFINE_REFS(DiskClient);
    DEFINE_ACCESS_SERVER(CallBackObj,MetaObj::none,MetaObj::none);
    DiskClient();
    static const uval BLOCKSIZE = 4096;
    virtual SysStatusUval getSize() { return diskSize; };
    virtual SysStatusUval getBlkSize() { return blkSize; }
    virtual SysStatus readBlock(uval blockNumber, void *block);
    virtual SysStatus writeBlock(uval blockNumber, void *block);

    virtual SysStatus readBlockPhys(uval blockNumber, uval paddr);
    virtual SysStatus writeBlockPhys(uval blockNumber, uval paddr);

    /* arg will indicate if it is a reply regarding physical or virtual
     * use mode, so we will know which transport to use */
    virtual __async SysStatusUval _processCallBacks(__in uval arg);

    virtual SysStatus getOH(ObjectHandle &oh) {
	oh = sbd.getOH();
	return 0;
    }
    static void ClassInit(VPNum vp);
    static SysStatus Create(DiskClientRef &dcr, char *blockDev,
			    uval needPhysAddr = 1);

#ifndef TOOLSIDE
    virtual SysStatus init(char *blockDev, uval needPhysAddr = 1);
    static uval Init(char *blockDev);
#else /* #ifndef TOOLSIDE */
    // non-zero result means new disk created, requires format
    static uval Init(char *diskFile);
#endif /* #ifndef TOOLSIDE */
};
#endif /* #ifndef __DISK_CLIENT_H_ */
