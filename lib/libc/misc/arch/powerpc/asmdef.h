#ifndef __ASMDEF_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ASMDEF_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: asmdef.h,v 1.9 2001/10/05 21:48:06 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: powerpc registers
 * **************************************************************************/

asm(".set r0,0");
asm(".set r1,1");
asm(".set r2,2");
asm(".set r3,3");
asm(".set r4,4");
asm(".set r5,5");
asm(".set r6,6");
asm(".set r7,7");
asm(".set r8,8");
asm(".set r9,9");
asm(".set r10,10");
asm(".set r11,11");
asm(".set r12,12");
asm(".set r13,13");
asm(".set r14,14");
asm(".set r15,15");
asm(".set r16,16");
asm(".set r17,17");
asm(".set r18,18");
asm(".set r19,19");
asm(".set r20,20");
asm(".set r21,21");
asm(".set r22,22");
asm(".set r23,23");
asm(".set r24,24");
asm(".set r25,25");
asm(".set r26,26");
asm(".set r27,27");
asm(".set r28,28");
asm(".set r29,29");
asm(".set r30,30");
asm(".set r31,31");

asm(".set f0,0");
asm(".set f1,1");
asm(".set f2,2");
asm(".set f3,3");
asm(".set f4,4");
asm(".set f5,5");
asm(".set f6,6");
asm(".set f7,7");
asm(".set f8,8");
asm(".set f9,9");
asm(".set f10,10");
asm(".set f11,11");
asm(".set f12,12");
asm(".set f13,13");
asm(".set f14,14");
asm(".set f15,15");
asm(".set f16,16");
asm(".set f17,17");
asm(".set f18,18");
asm(".set f19,19");
asm(".set f20,20");
asm(".set f21,21");
asm(".set f22,22");
asm(".set f23,23");
asm(".set f24,24");
asm(".set f25,25");
asm(".set f26,26");
asm(".set f27,27");
asm(".set f28,28");
asm(".set f29,29");
asm(".set f30,30");
asm(".set f31,31");

/* names of Condition Register fields */
asm(".set cr0,0");
asm(".set cr1,1");
asm(".set cr2,2");
asm(".set cr3,3");
asm(".set cr4,4");
asm(".set cr5,5");
asm(".set cr6,6");
asm(".set cr7,7");

/* bits within a CR field, for conditional branch tests */
asm(".set lt,0");
asm(".set gt,1");
asm(".set eq,2");
asm(".set so,3");

asm(".set sp,1");

asm(".set sprg0,0");
asm(".set sprg1,1");
asm(".set sprg2,2");
asm(".set sprg3,3");
