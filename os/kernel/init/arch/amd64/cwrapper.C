/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cwrapper.C,v 1.1 2001/09/14 15:48:39 pdb Exp $
 *****************************************************************************/
#include <sys/types.H>
#include "kernIncs.H"

extern sval
printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen);


extern "C" int  wrapper_printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen)
{
	int i;

	i = printfBuf(fmt0, argp, buf, buflen);
	return i;
}
