/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memcmp.C,v 1.9 2001/11/12 18:07:53 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <string.h>

int
memcmp(const void *str1, const void *str2, size_t n)
{
    if (n != 0) {
	char *cpstr1 = (char *)str1;
	char *cpstr2 = (char *)str2;

	do {
	    if (*cpstr1++ != *cpstr2++)
		return (*--cpstr1 - *--cpstr2);
	} while (--n != 0);
    }
    return 0;
}
