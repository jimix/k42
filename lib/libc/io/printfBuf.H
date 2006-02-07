#ifndef __PRINTF_BUF_H_
#define __PRINTF_BUF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printfBuf.H,v 1.15 2001/10/05 21:47:45 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines internal routines that all routines that
 * support printf format go to.  Can choose an arbitrary buf by
 * setting obj to zero.
 * **************************************************************************/

#include <misc/baseStdio.H>

extern sval
printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen);

extern sval
printfFile(const char *fmt0, va_list argp, FileLinuxRef ref);

#endif /* #ifndef __PRINTF_BUF_H_ */
