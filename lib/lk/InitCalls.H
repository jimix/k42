#ifndef __INITCALLS_H_
#define __INITCALLS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitCalls.H,v 1.1 2004/02/27 17:14:33 mostrows Exp $
 *****************************************************************************/
#include <misc/AutoList.H>
#include <alloc/alloc.H>

#define INITCALL(call)						\
    {								\
	extern initcall_t __initcall_##call;			\
	RegisterInitCall(new InitCall(&__initcall_##call));	\
    }


struct InitCall:public AutoListNode {
    DEFINE_GLOBAL_NEW(InitCall);
    void *addr;
    InitCall(void* ptr):addr(ptr) {};
};

extern SysStatus RegisterInitCall(InitCall *ic);
extern SysStatus RunInitCalls();
#endif //__INITCALLS_H_
