#ifndef __ASDEF_H_
#define __ASDEF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: asdef.h,v 1.10 2001/10/05 21:48:06 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PowerPC registers
 * **************************************************************************/

#define r0 0
#define r1 1
#define r2 2
#define r3 3
#define r4 4
#define r5 5
#define r6 6
#define r7 7
#define r8 8
#define r9 9
#define r10 10
#define r11 11
#define r12 12
#define r13 13
#define r14 14
#define r15 15
#define r16 16
#define r17 17
#define r18 18
#define r19 19
#define r20 20
#define r21 21
#define r22 22
#define r23 23
#define r24 24
#define r25 25
#define r26 26
#define r27 27
#define r28 28
#define r29 29
#define r30 30
#define r31 31

#define f0 0
#define f1 1
#define f2 2
#define f3 3
#define f4 4
#define f5 5
#define f6 6
#define f7 7
#define f8 8
#define f9 9
#define f10 10
#define f11 11
#define f12 12
#define f13 13
#define f14 14
#define f15 15
#define f16 16
#define f17 17
#define f18 18
#define f19 19
#define f20 20
#define f21 21
#define f22 22
#define f23 23
#define f24 24
#define f25 25
#define f26 26
#define f27 27
#define f28 28
#define f29 29
#define f30 30
#define f31 31

/* names of Condition Register fields */
#define cr0 0
#define cr1 1
#define cr2 2
#define cr3 3
#define cr4 4
#define cr5 5
#define cr6 6
#define cr7 7

/* bits within a CR field, for conditional branch tests */
#define lt 0
#define gt 1
#define eq 2
#define so 3

#define sp 1

#define sprg0 0
#define sprg1 1
#define sprg2 2
#define sprg3 3

#endif /* #ifndef __ASDEF_H_ */
