/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: g++sup.C,v 1.25 2001/10/08 22:21:35 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: support routines for C++ implementation
 * **************************************************************************/

#include <sys/sysIncs.H>

extern "C" void
__pure_virtual()
{
    err_printf("pure virtual method called\n");
    raiseError();
}

extern "C" void
__cxa_pure_virtual()
{
    err_printf("cxa pure virtual method called\n");
    raiseError();
}

void
__no_builtins_allowed(const char *classname, const char *op,
		      const char *f, int l)
{
    err_printf("%s:%d: C++ builtin: %s; called in class: %s\n",
	       f, l, op, classname);
    raiseError();
}

