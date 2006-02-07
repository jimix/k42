#ifndef __HOST_SYS_TYPES_H
#define __HOST_SYS_TYPES_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hostSysTypes.H,v 1.1 2004/01/29 23:02:57 aabauman Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: file for export uval/sval for building tools outside of
 *                     k42
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

// these need to be kept consistent with .../arch/types.H
typedef long			sval;
typedef unsigned long		uval;	// long necessary based on its use

// these need to be long long, not int64_t (which is long on 64-bit archs)
typedef long long		sval64;
typedef unsigned long long	uval64;

// the rest are defined in terms of the C99 fixed-size types
#include <inttypes.h> // should be <stdint.h> but AIX doesn't have it
typedef int8_t			sval8;
typedef int16_t			sval16;
typedef int32_t			sval32;
typedef uint8_t			uval8;
typedef uint16_t		uval16;
typedef uint32_t		uval32;

// FIXME: the rest of this stuff probably belongs elsewhere?
typedef uval		VPNum;
#define unlikely(x)	(x)	// Compiler hint defined in TAssert.H

#endif /* #ifndef __HOST_SYS_TYPES_H_ */
