#ifndef __TRACE_PACK_H_
#define __TRACE_PACK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: tracePack.H,v 1.6 2001/10/05 21:50:31 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions for tracing facility
 * **************************************************************************/

#define TRACE_1H_32_MASK 0x00000000ffffffffULL
#define TRACE_2H_32_MASK 0xffffffff00000000ULL
#define TRACE_1Q_16_MASK 0x000000000000ffffULL
#define TRACE_2Q_16_MASK 0x00000000ffff0000ULL
#define TRACE_3Q_16_MASK 0x0000ffff00000000ULL
#define TRACE_4Q_16_MASK 0xffff000000000000ULL
#define TRACE_1E_8_MASK 0x00000000000000ffULL
#define TRACE_2E_8_MASK 0x000000000000ff00ULL
#define TRACE_3E_8_MASK 0x0000000000ff0000ULL
#define TRACE_4E_8_MASK 0x00000000ff000000ULL
#define TRACE_5E_8_MASK 0x000000ff00000000ULL
#define TRACE_6E_8_MASK 0x0000ff0000000000ULL
#define TRACE_7E_8_MASK 0x00ff000000000000ULL
#define TRACE_8E_8_MASK 0xff00000000000000ULL

#define TRACE_PACK_32_32(a,b) ((uval64)                       \
    (((((uval64)(a))<<32) & TRACE_2H_32_MASK) |               \
      (((uval64)(b))      & TRACE_1H_32_MASK)))

#define TRACE_PACK_32_16_16(a,b,c) ((uval64)                  \
    (((((uval64)(a))<<32) & TRACE_2H_32_MASK) |               \
     ((((uval64)(b))<<16) & TRACE_2Q_16_MASK) |               \
      (((uval64)(c))      & TRACE_1Q_16_MASK)))

#define TRACE_PACK_32_16_8_8(a,b,c,d) ((uval64)               \
    (((((uval64)(a))<<32) & TRACE_2H_32_MASK) |               \
     ((((uval64)(b))<<16) & TRACE_2Q_16_MASK) |               \
     ((((uval64)(c))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(d))      & TRACE_1E_8_MASK)))

#define TRACE_PACK_32_8_8_8_8(a,b,c,d,e) ((uval64)            \
    (((((uval64)(a))<<32) & TRACE_2H_32_MASK) |               \
     ((((uval64)(b))<<24) & TRACE_4E_8_MASK)  |               \
     ((((uval64)(c))<<16) & TRACE_3E_8_MASK)  |               \
     ((((uval64)(d))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(e))      & TRACE_1E_8_MASK)))

#define TRACE_PACK_16_16_16_16(a,b,c,d) ((uval64)             \
    (((((uval64)(a))<<48) & TRACE_4Q_16_MASK) |               \
     ((((uval64)(b))<<32) & TRACE_3Q_16_MASK) |               \
     ((((uval64)(c))<<16) & TRACE_2Q_16_MASK) |               \
      (((uval64)(d))      & TRACE_1Q_16_MASK)))

#define TRACE_PACK_16_16_16_8_8(a,b,c,d,e) ((uval64)          \
    (((((uval64)(a))<<48) & TRACE_4Q_16_MASK) |               \
     ((((uval64)(b))<<32) & TRACE_3Q_16_MASK) |               \
     ((((uval64)(c))<<16) & TRACE_2Q_16_MASK) |               \
     ((((uval64)(d))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(e))      & TRACE_1E_8_MASK)))

#define TRACE_PACK_16_16_8_8_8_8(a,b,c,d,e,f) ((uval64)       \
    (((((uval64)(a))<<48) & TRACE_4Q_16_MASK) |               \
     ((((uval64)(b))<<32) & TRACE_3Q_16_MASK) |               \
     ((((uval64)(c))<<24) & TRACE_4E_8_MASK)  |               \
     ((((uval64)(d))<<16) & TRACE_3E_8_MASK)  |               \
     ((((uval64)(e))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(f))      & TRACE_1E_8_MASK)))

#define TRACE_PACK_16_8_8_8_8_8_8(a,b,c,d,e,f,g) ((uval64)    \
    (((((uval64)(a))<<48) & TRACE_4Q_16_MASK) |               \
     ((((uval64)(b))<<40) & TRACE_6E_8_MASK)  |               \
     ((((uval64)(c))<<32) & TRACE_5E_8_MASK)  |               \
     ((((uval64)(d))<<24) & TRACE_4E_8_MASK)  |               \
     ((((uval64)(e))<<16) & TRACE_3E_8_MASK)  |               \
     ((((uval64)(f))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(g))      & TRACE_1E_8_MASK)))

#define TRACE_PACK_8_8_8_8_8_8_8_8(a,b,c,d,e,f,g,h) ((uval64) \
    (((((uval64)(a))<<56) & TRACE_8E_8_MASK)  |               \
     ((((uval64)(b))<<48) & TRACE_7E_8_MASK)  |               \
     ((((uval64)(c))<<40) & TRACE_6E_8_MASK)  |               \
     ((((uval64)(d))<<32) & TRACE_5E_8_MASK)  |               \
     ((((uval64)(e))<<24) & TRACE_4E_8_MASK)  |               \
     ((((uval64)(f))<<16) & TRACE_3E_8_MASK)  |               \
     ((((uval64)(g))<<8)  & TRACE_2E_8_MASK)  |               \
      (((uval64)(h))      & TRACE_1E_8_MASK)))

#endif /* #ifndef __TRACE_PACK_H_ */
