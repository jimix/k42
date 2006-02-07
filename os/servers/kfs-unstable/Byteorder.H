#ifndef __BYTEORDER_H_
#define __BYTEORDER_H_

/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Byteorder.H,v 1.3 2004/08/20 17:30:46 mostrows Exp $
 *****************************************************************************/

// Only the tools have to deal with running on a host environment that is
// different from the target environment, so we only need endian-related
// operations ifdef KFS_TOOLS

#if defined (KFS_TOOLS) && \
    defined(PLATFORM_Linux_i686) && \
    !defined(__BIG_ENDIAN__)

#include <sys/bswap.h>
#define SWAB16(x)   bswap_16(x)
#define SWAB32(x)   bswap_32(x)
#define SWAB64(x)   bswap_64(x)

// Target Endian to CPU
#define TE16_TO_CPU(x)   SWAB16(x)
#define TE32_TO_CPU(x)   SWAB32(x)
#define TE64_TO_CPU(x)   SWAB64(x)

// CPU to Target Endian
#define CPU_TO_TE16(x)   SWAB16(x)
#define CPU_TO_TE32(x)   SWAB32(x)
#define CPU_TO_TE64(x)   SWAB64(x)

#define PRINT_TE64(v, nl) \
({ \
     uval64 __v = (v); \
     err_printf(" TE 0x %2x %2x %2x %2x %2x %2x %2x %2x ", \
		(unsigned int) (((uval64)__v) & (uval64)0x00000000000000ffULL),\
                (unsigned int) ((((uval64)__v) & (uval64)0x000000000000ff00ULL) >> 8),\
                (unsigned int) ((((uval64)__v) & (uval64)0x0000000000ff0000ULL) >> 16),\
                (unsigned int) ((((uval64)__v) & (uval64)0x00000000ff000000ULL) >> 24),\
                (unsigned int) ((((uval64)__v) & (uval64)0x000000ff00000000ULL) >> 32),\
                (unsigned int) ((((uval64)__v) & (uval64)0x0000ff0000000000ULL) >> 40),\
                (unsigned int) ((((uval64)__v) & (uval64)0x00ff000000000000ULL) >> 48),\
                (unsigned int) ((((uval64)__v) & (uval64)0xff00000000000000ULL) >> 56));\
     if (nl == 1) err_printf("\n");\
})

#define PRINT_TE32(v, nl) \
({ \
     uval32 __v = (v); \
     err_printf(" TE 0x %2x %2x %2x %2x", \
                (unsigned int) (((uval32)__v) & (uval32)0x000000ffUL),\
                (unsigned int) ((((uval32)__v) & (uval32)0x0000ff00UL) >> 8),\
                (unsigned int) ((((uval32)__v) & (uval32)0x00ff0000UL) >> 16),\
                (unsigned int) ((((uval32)__v) & (uval32)0xff000000UL) >> 24));\
     if (nl == 1) err_printf("\n");\
})

#define PRINT_TE16(v, nl) \
({ \
     uval16 __v = (v); \
     err_printf(" TE 0x %2x %2x", \
                (unsigned int) (((uval16)__v) & (uval16)0x00ff),\
                (unsigned int) ((((uval16)__v) & (uval16)0xff00) >> 8));\
     if (nl == 1) err_printf("\n");\
})


#else // #if defined (KFS_TOOLS) && defined(PLATFORM_Linux)

// no byte order change necessary, both host and target have the
// same endian (either host and target are powerpc,
// or host and target are x86)
#define TE_TO_CPU_IS_NOP

// Target Endian to CPU
#define TE16_TO_CPU(x)   (x)
#define TE32_TO_CPU(x)   (x)
#define TE64_TO_CPU(x)   (x)

// CPU to Target Endian
#define CPU_TO_TE16(x)   (x)
#define CPU_TO_TE32(x)   (x)
#define CPU_TO_TE64(x)   (x)

#define PRINT_TE64(v, nl)    err_printf(" TE simple 0x%llx ", v);\
                             if (nl == 1) err_printf("\n");
#define PRINT_TE32(v, nl)    err_printf(" TE simple 0x%x ", v);\
                             if (nl == 1) err_printf("\n");
#define PRINT_TE16(v, nl)    err_printf(" TE simple 0x%x ", v);\
                             if (nl == 1) err_printf("\n");


#endif // #if defined (KFS_TOOLS) && defined(PLATFORM_Linux)

#endif // #ifndef __BYTEORDER_H_
