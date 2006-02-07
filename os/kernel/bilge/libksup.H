#ifndef __LIBKSUP_H_
#define __LIBKSUP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libksup.H,v 1.6 2001/10/05 21:49:11 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides machine independant interface to basic
 * operations like printing..
 * **************************************************************************/

sval localPeekChar(void);
void localPutChar(char c);
void baseAbort(void);
void breakpoint(void);
// printf to be used only for early initialization
void init_printf(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

#endif /* #ifndef __LIBKSUP_H_ */
