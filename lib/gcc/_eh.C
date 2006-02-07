/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: _eh.C,v 1.1 2001/06/26 14:59:34 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "_eh.h"

void
__default_terminate()
{
    passert(0, err_printf("%s: called.\n", __FUNCTION__));
}

void (*__terminate_func)() = __default_terminate;
void
__terminate()
{
    (*__terminate_func)();
}

void *
__throw_type_match(void *catch_type, void *throw_type, void *obj)
{
    if (strcmp((const char *)catch_type, (const char *)throw_type) == 0)
	return obj;
    return 0;
}

void
__empty()
{
}


void *
__get_eh_context()
{
    return (void *) (*get_eh_context)();
}

void **
__get_eh_info()
{
    struct eh_context *eh = (*get_eh_context)();
    return &eh->info;
}

extern "C" struct eh_context *
eh_context_initialize()
{
    get_eh_context = &eh_context_static;
    return (*get_eh_context)();
}

extern "C" {
struct eh_context *(*get_eh_context)() = &eh_context_initialize;
}

struct eh_context *
eh_context_static()
{
    static struct eh_context eh;
    static int initialized;
    static void *top_elt[2];
    if (! initialized)
    {
	initialized = 1;
	memset(&eh, 0, sizeof eh);
	eh.dynamic_handler_chain = top_elt;
    }
    return &eh;
}

void ***
__get_dynamic_handler_chain()
{
    struct eh_context *eh = (*get_eh_context)();
    return &eh->dynamic_handler_chain;
}

void
__sjthrow()
{
    struct eh_context *eh = (*get_eh_context)();
    void ***dhc = &eh->dynamic_handler_chain;
    void * __jmpbuf ;
    void (*func)(void *, int);
    void *arg;
    void ***cleanup = (void***)&(*dhc)[1];

    if (cleanup[0])
    {
	double store[200];
	void **buf = (void**)store;
	buf[1] = 0;
	buf[0] = (*dhc);
	if (! __builtin_setjmp(&buf[2]))
	{
	    *dhc = buf;
	    while (cleanup[0])
	    {
		func = (void(*)(void*, int))cleanup[0][1];
		arg = (void*)cleanup[0][2];
		cleanup[0] = (void **)cleanup[0][0];
		(*func)(arg, 2);
	    }
	    *dhc = (void **)buf[0];
	}
	else
	{
	    __terminate();
	}
    }
    if (! eh->info || (*dhc)[0] == 0)
	__terminate();
    __jmpbuf  = &(*dhc)[2];
    *dhc = (void**)(*dhc)[0];
    __builtin_longjmp(__jmpbuf , 1);
}

void
__sjpopnthrow()
{
    struct eh_context *eh = (*get_eh_context)();
    void ***dhc = &eh->dynamic_handler_chain;
    void (*func)(void *, int);
    void *arg;
    void ***cleanup = (void***)&(*dhc)[1];

    if (cleanup[0])
    {
	double store[200];
	void **buf = (void**)store;
	buf[1] = 0;
	buf[0] = (*dhc);
	if (! __builtin_setjmp(&buf[2]))
	{
	    *dhc = buf;
	    while (cleanup[0])
	    {
		func = (void(*)(void*, int))cleanup[0][1];
		arg = (void*)cleanup[0][2];
		cleanup[0] = (void **)cleanup[0][0];
		(*func)(arg, 2);
	    }
	    *dhc = (void **)buf[0];
	}
	else
	{
	    __terminate();
	}
    }
    *dhc = (void**)(*dhc)[0];
    __sjthrow();
}

int
__eh_rtime_match(void *rtime)
{
    void *info;
    __eh_matcher matcher;
    void *ret;
    info = *(__get_eh_info());
    matcher = ((__eh_info *)info)->match_function;

    passert(matcher != NULL,
	    err_printf("Internal Compiler Bug: No runtime type matcher."));

    ret = (*matcher)(info, rtime, (void *)0);
    return (ret != NULL);
}

