/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ctype.h,v 1.11 2000/05/11 11:29:07 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: ANSI C functionlity.  Note, adding features as
 * required. Current implementations are real in-efficient, should have
 * in-line versions
 * **************************************************************************/

#ifndef NETBSD_KERNELINCS_DEFH
/* get linux headers */
#ifndef _CTYPE_H
#include_next <ctype.h>
#endif

#else // NETBSD_KERNELINCS_DEFH
#ifndef CTYPE_DEFH
#define CTYPE_DEFH

#include <sys/types.H>

// FIXME: figure out what definitions we need here
typedef unsigned short	wchar_t;
typedef unsigned int	wctype_t;
typedef int	wint_t;

__BEGIN_C_DECLS
int isalnum(int c);
int isalpha(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int isascii(int c);
__END_C_DECLS

#endif /* CTYPE_DEFH */
#endif // NETBSD_KERNELINCS_DEFH
