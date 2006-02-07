/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ConsoleBuffer.H,v 1.2 2002/10/10 13:08:31 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Represent console input as a ring buffer, so that
		       data can be inserted as input from non-console sources
 ****************************************************************************/
#ifndef __CONSOLE_BUFFER_H_
#define __CONSOLE_BUFFER_H_

#include <io/IORingBuffer.H>
#include <alloc/alloc.H>
#include <sync/FairBLock.H>
#include <sync/Sem.H>

struct ConsoleBuffer:protected IORingBuffer{
    DEFINE_PINNEDGLOBAL_NEW(ConsoleBuffer);
    FairBLock lock;
    Semaphore sem;
    void read();
    static void Read(uval t);
    friend void insertToConsole(char* buf, uval len);
    void insert(char* buf, uval len);
public:
    static ConsoleBuffer* console;
    static void ClassInit(VPNum vp);
    virtual void init(uval bufSize);
    virtual uval getData(char* data, uval size);
};



#endif /* #ifndef __CONSOLE_BUFFER_H_ */
