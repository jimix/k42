/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memccpy.C,v 1.6 2000/05/11 11:29:07 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <string.h>

void *
memccpy(void *to, const void *from, int c, size_t n)
{
    if (n) {
	unsigned char *tptr = (unsigned char *)to;
	const unsigned char *fptr = (unsigned char *)from;
	unsigned char uc = c;
	do {
	    if ((*tptr++ = *fptr++) == uc)
		return (tptr);
	} while (--n != 0);
    }
    return (0);
}
