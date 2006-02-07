/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: _eprintf.C,v 1.1 2001/06/26 14:59:34 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Support for abort
 * **************************************************************************/
#include <sys/sysIncs.H>

/* This is used by the standard C++ `assert' macro.  */
extern "C" void __eprintf (const char *, const char *,
			   unsigned int, const char *);
void
__eprintf(const char *string, const char *expression,
	   unsigned int line, const char *filename)
{
    passert(0, err_printf(string, expression, line, filename));
}
