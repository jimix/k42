#ifndef __KERNEL_INFO_MGR_H_
#define __KERNEL_INFO_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelInfoMgr.H,v 1.9 2002/10/10 13:08:50 rosnbrg Exp $
 *****************************************************************************/

#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

class KernelInfoMgr: public Obj {
protected:
    BLock lock;
    //FIXME MAA we shouldn't have to keep a private map of
    //          active kernel vp's, should we?
    VPSet vpset;
    // base copy - only the published part matters
    // we update published info here and publish
    struct KernelInfo::SystemGlobal systemGlobal;
    virtual SysStatus KernelInfoMgr::initVP(VPNum vp);

public:
    DEFINE_PINNEDGLOBAL_NEW(KernelInfoMgr);
    static void ClassInit(VPNum vp);
    virtual SysStatus lockAndGetPtr(KernelInfo::SystemGlobal*& sgp);
    //unlock if you've just used pointer to read data
    virtual SysStatus unlock();
    //publichAndUnlock if you've updated
    virtual SysStatus publishAndUnlock();

    static void SetControl(uval ctrlFlags);
    static void SetControlBit(uval ctrlBit, uval value);
};
#endif /* #ifndef __KERNEL_INFO_MGR_H_ */
