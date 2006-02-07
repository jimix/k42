/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceK42.h,v 1.2 2004/04/21 13:15:48 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: external tracing includes
 * **************************************************************************/

#include <stdarg.h>

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

#undef NULL
#define NULL 0

#define err_printf(strargs...) 
#define tassert(x, y)
#define passert(x, y)

#define unlikely(x)	__builtin_expect((x),0)

#define TRACE_INFO (TraceInfo *)0xe0000000000fe048
#define TRACE_MASK (TraceInfo *)0xe0000000000fe048

#include "trace/traceCore.H"

typedef struct {
    TraceInfo traceInfo;
} KernelInfoFake;

KernelInfoFake kernelInfoLocal;

#define kernelInfoLocal (*((KernelInfoFake *) TRACE_INFO))
