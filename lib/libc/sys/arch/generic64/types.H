#ifndef __TYPES_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __TYPES_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: types.H,v 1.2 2001/10/05 21:48:41 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Include TYPES from machine specific file,
 * the basic types that should be defined are:
 *   sval          - signed value of the natural machine type
 *   uval          - unsigned value of the natural machine type,
 *                   is large enough to hold a pointer
 *   sval8/uval8   - signed/unsigned 8 bit quantities
 *   sval16/uval16 - signed/unsigned 16 bit quantities
 *   sval32/uval32 - signed/unsigned 32 bit quantities
 *   sval64/uval64 - signed/unsigned 64 bit quantities
 *
 ***************************************************************************/

#define _SIZEUVAL 8		/* size UVAL in bytes */
typedef long			sval;
typedef signed char		sval8;
typedef short			sval16;
typedef int			sval32;
typedef long long 		sval64;

typedef unsigned long		uval;
typedef unsigned char		uval8;
typedef unsigned short		uval16;
typedef unsigned int		uval32;
typedef unsigned long long	uval64;
