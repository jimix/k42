#ifndef __IOVEC_H_
#define __IOVEC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: iovec.h,v 1.1 2001/11/01 19:54:06 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Utility functions for scatter/gather
 * **************************************************************************/
#include <sys/uio.h>

extern uval memcpy_fromiovec(char *buf, struct iovec* vec, uval iovcount);
extern uval memcpy_toiovec(struct iovec* vec, char* buf,
			   uval iovcount, uval buflen);

#endif /* #ifndef __IOVEC_H_ */
