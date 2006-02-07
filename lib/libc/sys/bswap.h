#ifndef __BSWAP_H_
#define __BSWAP_H_
/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bswap.h,v 1.2 2004/08/20 17:30:44 mostrows Exp $
 *****************************************************************************/

/* this file defines byte swapping routines: bswap_16, bswap_32 and bswap_64 */

#if !defined(KERNEL) && defined(PLATFORM_Linux_i686)
// use optimised routines found in Linux headers
# include <byteswap.h>
#else

// generic routines
// FIXME: we should use architecture-dependent optmized versions
#define bswap_16(x) \
({ \
	uval16 __x = (x); \
	((uval16)( \
	    (((uval16)(__x) & (uval16)0x00ffU) << 8) | \
	    (((uval16)(__x) & (uval16)0xff00U) >> 8) )); \
})

#define bswap_32(x) \
({ \
	uval32 __x = (x); \
	((uval32)( \
	    (((uval32)(__x) & (uval32)0x000000ffUL) << 24) | \
	    (((uval32)(__x) & (uval32)0x0000ff00UL) <<  8) | \
	    (((uval32)(__x) & (uval32)0x00ff0000UL) >>  8) | \
	    (((uval32)(__x) & (uval32)0xff000000UL) >> 24) )); \
})

#define bswap_64(x) \
({ \
	uval64 __x = (x); \
	((uval64)( \
	    (uval64)(((uval64)(__x) & (uval64)0x00000000000000ffULL) << 56) | \
	    (uval64)(((uval64)(__x) & (uval64)0x000000000000ff00ULL) << 40) | \
	    (uval64)(((uval64)(__x) & (uval64)0x0000000000ff0000ULL) << 24) | \
	    (uval64)(((uval64)(__x) & (uval64)0x00000000ff000000ULL) <<  8) | \
	    (uval64)(((uval64)(__x) & (uval64)0x000000ff00000000ULL) >>  8) | \
	    (uval64)(((uval64)(__x) & (uval64)0x0000ff0000000000ULL) >> 24) | \
	    (uval64)(((uval64)(__x) & (uval64)0x00ff000000000000ULL) >> 40) | \
	    (uval64)(((uval64)(__x) & (uval64)0xff00000000000000ULL) >> 56) ));\
})

#endif

#endif /* !defined(__BSWAP_H_) */
