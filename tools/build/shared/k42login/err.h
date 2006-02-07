#ifndef __ERR_H_
#define __ERR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: err.h,v 1.3 2004/01/30 21:58:23 aabauman Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: rlogin to K42 ports
 * **************************************************************************/

#include <stdio.h>

#define warn printf
#define warnx printf

#include <stdarg.h>

void
err (int eval, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	printf(fmt, ap);

	va_end(ap);
}

void
errx (int eval, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	printf(fmt, ap);

	va_end(ap);
}

#endif /* #ifndef __ERR_H_ */
