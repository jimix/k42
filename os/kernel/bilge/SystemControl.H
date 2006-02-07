/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemControl.H,v 1.1 2004/01/16 16:37:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Represent system control requests as a ring
 * buffer, so that data can be inserted as input from non-console
 * sources. Eventually move the actual command loop here to get out of
 * KernelInit.C
 ****************************************************************************/
#ifndef __SYSTEM_CONTROL_H_
#define __SYSTEM_CONTROL_H_

#include <io/IORingBuffer.H>
#include <alloc/alloc.H>
#include <sync/FairBLock.H>
#include <scheduler/Scheduler.H>
#include <sync/Sem.H>

class SystemControl : protected IORingBuffer{
    DEFINE_PINNEDGLOBAL_NEW(SystemControl);
    FairBLock lock;
    Semaphore sem;
    static SystemControl* systemControl;
    uval attachedToConsole;

    /* operations that pull data by default from console */
    void readFromConsole();		// reads from real console
    static void ReadFromConsole(uval t); // reads from real console

    void insert(char* buf, uval len);

    virtual void init(uval bufSize);

    virtual uval read(char* data, uval size); // read from ring buffer

public:

    static void ClassInit(VPNum vp);

    /* read control operation */
    static uval Read(char *str, uval len);

    /* insert control operation into buffer */
    static void Insert(char* buf, uval len);

    static void DetachFromConsole();
    static void AttachToConsole();
};



#endif /* #ifndef __SYSTEM_CONTROL_H_ */
