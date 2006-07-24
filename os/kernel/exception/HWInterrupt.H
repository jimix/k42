#ifndef __HWINTERRUPT_H_
#define __HWINTERRUPT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.H,v 1.33 2004/02/27 17:14:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent interrupt functionality
 * **************************************************************************/

#include "exception/ExceptionLocal.H"
#include <sync/MPMsgMgr.H>
//K42_LINUX
#define ICU_LEN         32

#define IST_LEVEL       3

extern ExceptionLocal exceptionLocal;
//K42_LINUX
extern "C" void HWInterrupt_IOInterrupt(uval interruptNum);

class HWInterrupt {
public:
    static HWInterrupt *theHWInterrupt;
protected:
    HWInterrupt() { /* empty body */ }
    BLock lock;
    friend void HWInterrupt_IOInterrupt(uval interruptNum);
    static void (*entryPoint)(uval intr);
public:

    DEFINE_PINNEDGLOBALPADDED_NEW(HWInterrupt);

    // Class code : The code used by the class to distinguish various errors
    enum {
        ERR_INVALID_ARGS   = 1,
        ERR_NOMEM          = 2,
        ERR_NOHANDLER      = 3,
        ERR_NOTIMPLEMENTED = 4
    };

    uval interruptCount;

    virtual void __prepareStartCPU(uval num) {};
    static void PrepareStartCPU(uval num) {
	theHWInterrupt->__prepareStartCPU(num);
    }
    static void SecondaryPreInit(VPNum vp);
    static void ClassInit(VPNum vp);

    static void HandlerStats();
    static void HandlerStats(uval vector);

    virtual void sendIPI(VPNum vp) {
	passertMsg(0,"No implementation for sendIPI");
    }
    static  void SendIPI(VPNum vp);
    virtual void CPUReady(VPNum vp) {
	passertMsg(0,"No implementation for CPUReady");
    }
    virtual void CPUDead(uval killGlobal);
    virtual void init();

    virtual ~HWInterrupt() { }

private:
    static uval SecondaryPrompt(uval maxSecondary);
    static HWInterrupt* controller;

#define INCLASS
#include __MINC(HWInterrupt.H)
#undef INCLASS
};

#include __MINC(HWInterrupt.H)

extern "C" void HWInterrupt_IOInterrupt(uval interruptNum);
extern "C" void linuxCheckSoftIntrs(uval calledFromInterrupt);

#endif /* #ifndef __HWINTERRUPT_H_ */
