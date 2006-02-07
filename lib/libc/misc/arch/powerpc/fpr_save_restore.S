/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fpr_save_restore.S,v 1.6 2000/08/07 14:10:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Routines, required by the ABI, for saving and restoring floating point
 *    registers.
 * **************************************************************************/

#include <sys/kinclude.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>

#define STK_F(n) (-((32-(n))*8))

C_TEXT_ENTRY(_savefXX)	
C_TEXT_BASIC_ENTRY(_savef14);	stfd f14,STK_F(14)(r1)				
C_TEXT_BASIC_ENTRY(_savef15);	stfd f15,STK_F(15)(r1)
C_TEXT_BASIC_ENTRY(_savef16);	stfd f16,STK_F(16)(r1)
C_TEXT_BASIC_ENTRY(_savef17);	stfd f17,STK_F(17)(r1)
C_TEXT_BASIC_ENTRY(_savef18);	stfd f18,STK_F(18)(r1)
C_TEXT_BASIC_ENTRY(_savef19);	stfd f19,STK_F(19)(r1)
C_TEXT_BASIC_ENTRY(_savef20);	stfd f20,STK_F(20)(r1)
C_TEXT_BASIC_ENTRY(_savef21);	stfd f21,STK_F(21)(r1)
C_TEXT_BASIC_ENTRY(_savef22);	stfd f22,STK_F(22)(r1)
C_TEXT_BASIC_ENTRY(_savef23);	stfd f23,STK_F(23)(r1)
C_TEXT_BASIC_ENTRY(_savef24);	stfd f24,STK_F(24)(r1)
C_TEXT_BASIC_ENTRY(_savef25);	stfd f25,STK_F(25)(r1)
C_TEXT_BASIC_ENTRY(_savef26);	stfd f26,STK_F(26)(r1)
C_TEXT_BASIC_ENTRY(_savef27);	stfd f27,STK_F(27)(r1)
C_TEXT_BASIC_ENTRY(_savef28);	stfd f28,STK_F(28)(r1)
C_TEXT_BASIC_ENTRY(_savef29);	stfd f29,STK_F(29)(r1)
C_TEXT_BASIC_ENTRY(_savef30);	stfd f30,STK_F(30)(r1)
C_TEXT_BASIC_ENTRY(_savef31);	stfd f31,STK_F(31)(r1)
				blr
C_TEXT_END(_savefXX)

C_TEXT_ENTRY(_restfXX)	
C_TEXT_BASIC_ENTRY(_restf14);	lfd f14,STK_F(14)(r1)
C_TEXT_BASIC_ENTRY(_restf15);	lfd f15,STK_F(15)(r1)
C_TEXT_BASIC_ENTRY(_restf16);	lfd f16,STK_F(16)(r1)
C_TEXT_BASIC_ENTRY(_restf17);	lfd f17,STK_F(17)(r1)
C_TEXT_BASIC_ENTRY(_restf18);	lfd f18,STK_F(18)(r1)
C_TEXT_BASIC_ENTRY(_restf19);	lfd f19,STK_F(19)(r1)
C_TEXT_BASIC_ENTRY(_restf20);	lfd f20,STK_F(20)(r1)
C_TEXT_BASIC_ENTRY(_restf21);	lfd f21,STK_F(21)(r1)
C_TEXT_BASIC_ENTRY(_restf22);	lfd f22,STK_F(22)(r1)
C_TEXT_BASIC_ENTRY(_restf23);	lfd f23,STK_F(23)(r1)
C_TEXT_BASIC_ENTRY(_restf24);	lfd f24,STK_F(24)(r1)
C_TEXT_BASIC_ENTRY(_restf25);	lfd f25,STK_F(25)(r1)
C_TEXT_BASIC_ENTRY(_restf26);	lfd f26,STK_F(26)(r1)
C_TEXT_BASIC_ENTRY(_restf27);	lfd f27,STK_F(27)(r1)
C_TEXT_BASIC_ENTRY(_restf28);	lfd f28,STK_F(28)(r1)
C_TEXT_BASIC_ENTRY(_restf29);	lfd f29,STK_F(29)(r1)
C_TEXT_BASIC_ENTRY(_restf30);	lfd f30,STK_F(30)(r1)
C_TEXT_BASIC_ENTRY(_restf31);	lfd f31,STK_F(31)(r1)
				blr
C_TEXT_END(_restfXX)
