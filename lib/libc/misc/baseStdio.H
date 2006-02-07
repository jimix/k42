#ifndef __BASE_STDIO_H_
#define __BASE_STDIO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: baseStdio.H,v 1.12 2004/06/28 17:01:23 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

class   FileLinux;
typedef FileLinux **FileLinuxRef;

char	*baseGets(char *);
SysStatus baseFgets(char *, sval, FileLinuxRef);
sval	 baseSprintf(char *, const char *, ...)
	    __attribute__ ((format (printf, 2, 3)));


#endif /* #ifndef __BASE_STDIO_H_ */
