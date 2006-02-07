/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: string.h,v 1.13 2001/03/22 18:02:55 jimix Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#ifndef NETBSD_KERNELINCS_DEFH
/* get linux headers */
#ifndef _STRING_H
#include_next <string.h>
#endif

#else // NETBSD_KERNELINCS_DEFH
#ifndef STRING_DEFH
#define STRING_DEFH

#include <sys/kinclude.H>

/* Avoid stddef.h definition of size_t until G++ __null lossage fixed */
#if 0
#include <stddef.h>
#endif

#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
typedef long unsigned int size_t;
#endif

__BEGIN_C_DECLS
void	*memset(void * t, int c, size_t count);
int	strncmp(const char * a, const char * b, size_t max);
int	strcmp(const char * a, const char * b);
size_t	strlen(const char *s);
size_t  strnlen(const char *s, size_t len);
void	*memcpy(void *s1, const void *s2, size_t len);
void	*memmove(void *s1, const void *s2, size_t len);
char	*strcpy(char *s1, const char *s2);
char	*strncpy(char *s1, const char *s2, size_t len);
char	*strcat(char *s1, const char *s2);
char	*strchr(const char *s, int c);
char	*strrchr(const char *s, int c);
void	*memchr(const void *p, int c, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);

// non standard ANSI
void	*memccpy(void *, const void *, int, size_t);
__END_C_DECLS

#endif // STRING_DEFH

#endif // NETBSD_KERNELINCS_DEFH
