#ifndef __LOCAL_CONSOLE_H_
#define __LOCAL_CONSOLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LocalConsole.H,v 1.5 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A simple line buffered object local to each
 * processor for handling local console I/O.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>
#include <io/IORingBuffer.H>
#include <sys/IOChan.H>

#define MAX_LINEBUF 64

class LocalConsole {

public:
    IOChan* ioc; // Channel for console prompt and tty
    IOChan* raw; // Channel for err_printf
protected:
    typedef BLock LockType;
    LockType	      rdlock;
    LockType	      wtlock;

    IORingBuffer buffer;
    // buffer management routines
    void init(MemoryMgrPrimitive* mem, IOChan* chan);
public:

    /* Sets index of IOChans::channels to use for a console */
    void setConsole(IOChan* ioc);

    SysStatusUval read(char *buf, uval len);
    SysStatusUval write(const char *buf, uval len);

    char peekChar();
    SysStatusUval getChar(char &c);
    void putChar(char c);

    static void Init(VPNum vp, MemoryMgrPrimitive* mem, IOChan* chan);

    DEFINE_PRIMITIVE_NEW(LocalConsole);
};

extern LocalConsole* SysConsole;

#endif /* #ifndef __LOCAL_CONSOLE_H_ */
