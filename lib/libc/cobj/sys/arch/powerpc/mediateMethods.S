/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mediateMethods.S,v 1.8 2004/06/09 17:12:28 andrewb Exp $
 *****************************************************************************/
#include <sys/kinclude.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>

// define the non-volatile register being used
#define NVREG		r31

// layout of the stack frame saved below the stack pointer across mediator calls
#define LSTK_F1		((-1)*8)
#define LSTK_F2		((-2)*8)
#define LSTK_F3		((-3)*8)
#define LSTK_F4		((-4)*8)
#define LSTK_F5		((-5)*8)
#define LSTK_F6		((-6)*8)
#define LSTK_F7		((-7)*8)
#define LSTK_F8		((-8)*8)
#define LSTK_F9		((-9)*8)
#define LSTK_F10	((-10)*8)
#define LSTK_F11	((-11)*8)
#define LSTK_F12	((-12)*8)
#define LSTK_F13	((-13)*8)
#define LSTK_NVREG	((-14)*8)
#define LSTK_DOEPILOG	((-15)*8)
#define LSTK_SIZE	(STK_SIZE + 15*8)

CODE_ENTRY(mediateMethodCommon)
	mflr	r0				// save link register
	std	r0,STK_LR(r1)
	std	r3,STK_PARAM0(r1)		// save parameter gprs
	std	r4,STK_PARAM1(r1)
	std	r5,STK_PARAM2(r1)
	std	r6,STK_PARAM3(r1)
	std	r7,STK_PARAM4(r1)
	std	r8,STK_PARAM5(r1)
	std	r9,STK_PARAM6(r1)
	std	r10,STK_PARAM7(r1)
	stfd	f1,LSTK_F1(r1)			// save parameter fprs
	stfd	f2,LSTK_F2(r1)
	stfd	f3,LSTK_F3(r1)
	stfd	f4,LSTK_F4(r1)
	stfd	f5,LSTK_F5(r1)
	stfd	f6,LSTK_F6(r1)
	stfd	f7,LSTK_F7(r1)
	stfd	f8,LSTK_F8(r1)
	stfd	f9,LSTK_F9(r1)
	stfd	f10,LSTK_F10(r1)
	stfd	f11,LSTK_F11(r1)
	stfd	f12,LSTK_F12(r1)
	stfd	f13,LSTK_F13(r1)

	std	NVREG,LSTK_NVREG(r1)		// also save nvreg on stack for
						// when epilog is not needed

	la	r3,STK_PARAM0(r1)		// first param:  &this
	mr	r4,r12				// second param: method number
	mr	r5,r0				// third param:  real ret addr
	mr	r6,NVREG			// fourth param: our nvreg
	ld	NVREG,STK_PARAM0(r1)		// save mediator ptr to nvreg
	la	r7,LSTK_DOEPILOG(r1)		// fifth param:  &doEpilog
	stdu	r1,-LSTK_SIZE(r1)		// push frame
	bl	C_TEXT(mediateForwardProlog)	// invoke forward prolog
	la	r1,LSTK_SIZE(r1)		// pop frame
	ld	r2,8(r3)			// load toc from func desc
	ld	r3,0(r3)			// load ctr from func desc
	mtctr	r3
	ld	r0,STK_LR(r1)			// restore link register
	mtlr	r0
	ld	r3,STK_PARAM0(r1)		// restore parameter gprs
	ld	r4,STK_PARAM1(r1)
	ld	r5,STK_PARAM2(r1)
	ld	r6,STK_PARAM3(r1)
	ld	r7,STK_PARAM4(r1)
	ld	r8,STK_PARAM5(r1)
	ld	r9,STK_PARAM6(r1)
	ld	r10,STK_PARAM7(r1)
	lfd	f1,LSTK_F1(r1)			// restore parameter fprs
	lfd	f2,LSTK_F2(r1)
	lfd	f3,LSTK_F3(r1)
	lfd	f4,LSTK_F4(r1)
	lfd	f5,LSTK_F5(r1)
	lfd	f6,LSTK_F6(r1)
	lfd	f7,LSTK_F7(r1)
	lfd	f8,LSTK_F8(r1)
	lfd	f9,LSTK_F9(r1)
	lfd	f10,LSTK_F10(r1)
	lfd	f11,LSTK_F11(r1)
	lfd	f12,LSTK_F12(r1)
	lfd	f13,LSTK_F13(r1)

	ld	r0,LSTK_DOEPILOG(r1)		// Observe out param (doEpilog)
	cmpdi	r0,0
	beq	noEpilog			// b if doEpilog is zero

	bctrl					// Call method with orig stk

						// Prepare for epilog
	std	r3,STK_PARAM0(r1)		// save result gpr
	stfd	f1,LSTK_F1(r1)			// save result fprs
	stfd	f2,LSTK_F2(r1)
	stfd	f3,LSTK_F3(r1)
	stfd	f4,LSTK_F4(r1)
	mr	r3,NVREG			// first param:  mediator ptr
	la	r4,LSTK_NVREG(r1)		// second param: &nvreg


	/* (re)load the TOC for this module
         * ================================
         * This is nasty, but the method we just called could have a different
         * TOC pointer, so we need to reload the mediator object's TOC.
         * Unfortunately we don't have a stack frame to save it in across the
         * call, so we need to load it from scratch. We also can't load it from
         * any symbol/function pointer in the data segment, because we need the
         * TOC indirection to get there, so we load it relative to the PC from a
         * constant embedded in the code, which we jump over. */

        .align 3	// inserts 0 or 1 nop instructions to achieve alignment
	/* the next instruction is now 64-bit aligned */
	stdu	r1,-LSTK_SIZE(r1)		// push frame
	bl	1f				// jump over TOC word setting LR
        /* here is again 64-bit aligned, due to the two 4-byte instructions */
	.quad	.TOC.@tocbase			// the TOC pointer (data)
1:	mflr	r2				// now r2 points to the TOC word
	ld	r2,0(r2)			// load new TOC


	bl	C_TEXT(mediateForwardEpilog)	// invoke forward epilog
	la	r1,LSTK_SIZE(r1)		// pop frame
	mtlr	r3				// get ra loaded first
	ld	NVREG,LSTK_NVREG(r1)		// restore nvreg also
	ld	r3,STK_PARAM0(r1)		// restore result gpr
	lfd	f1,LSTK_F1(r1)			// restore result fprs
	lfd	f2,LSTK_F2(r1)
	lfd	f3,LSTK_F3(r1)
	lfd	f4,LSTK_F4(r1)

	blr					// return

noEpilog:					// Simply forward call,
						// bypassing the epilog.
	ld	NVREG,LSTK_NVREG(r1)		// restore nvreg from stack

	bctr					// Use bctr to redirect call
						// (won't return here)
	tw	31,0,0				// NOTREACHED
CODE_END(mediateMethodCommon)

#define MEDIATE_METHOD(OP)\
    C_TEXT_BASIC_ENTRY(mediateMethod ## OP);\
	li	r12,OP;\
	b	mediateMethodCommon;\
    C_TEXT_BASIC_END(mediateMethod ## OP)

MEDIATE_METHOD(0) // Not sure we actually want this as the first vtable seems
                // empty or atleast reserved for other uses, but for the sake
                // of symmetry
MEDIATE_METHOD(1)
MEDIATE_METHOD(2)
MEDIATE_METHOD(3)
MEDIATE_METHOD(4)
MEDIATE_METHOD(5)
MEDIATE_METHOD(6)
MEDIATE_METHOD(7)
MEDIATE_METHOD(8)
MEDIATE_METHOD(9)

MEDIATE_METHOD(10);
MEDIATE_METHOD(11);
MEDIATE_METHOD(12);
MEDIATE_METHOD(13);
MEDIATE_METHOD(14);
MEDIATE_METHOD(15);
MEDIATE_METHOD(16);
MEDIATE_METHOD(17);
MEDIATE_METHOD(18);
MEDIATE_METHOD(19);

MEDIATE_METHOD(20);
MEDIATE_METHOD(21);
MEDIATE_METHOD(22);
MEDIATE_METHOD(23);
MEDIATE_METHOD(24);
MEDIATE_METHOD(25);
MEDIATE_METHOD(26);
MEDIATE_METHOD(27);
MEDIATE_METHOD(28);
MEDIATE_METHOD(29);

MEDIATE_METHOD(30);
MEDIATE_METHOD(31);
MEDIATE_METHOD(32);
MEDIATE_METHOD(33);
MEDIATE_METHOD(34);
MEDIATE_METHOD(35);
MEDIATE_METHOD(36);
MEDIATE_METHOD(37);
MEDIATE_METHOD(38);
MEDIATE_METHOD(39);

MEDIATE_METHOD(40);
MEDIATE_METHOD(41);
MEDIATE_METHOD(42);
MEDIATE_METHOD(43);
MEDIATE_METHOD(44);
MEDIATE_METHOD(45);
MEDIATE_METHOD(46);
MEDIATE_METHOD(47);
MEDIATE_METHOD(48);
MEDIATE_METHOD(49);

MEDIATE_METHOD(50);
MEDIATE_METHOD(51);
MEDIATE_METHOD(52);
MEDIATE_METHOD(53);
MEDIATE_METHOD(54);
MEDIATE_METHOD(55);
MEDIATE_METHOD(56);
MEDIATE_METHOD(57);
MEDIATE_METHOD(58);
MEDIATE_METHOD(59);

MEDIATE_METHOD(60);
MEDIATE_METHOD(61);
MEDIATE_METHOD(62);
MEDIATE_METHOD(63);
MEDIATE_METHOD(64);
MEDIATE_METHOD(65);
MEDIATE_METHOD(66);
MEDIATE_METHOD(67);
MEDIATE_METHOD(68);
MEDIATE_METHOD(69);

MEDIATE_METHOD(70);
MEDIATE_METHOD(71);
MEDIATE_METHOD(72);
MEDIATE_METHOD(73);
MEDIATE_METHOD(74);
MEDIATE_METHOD(75);
MEDIATE_METHOD(76);
MEDIATE_METHOD(77);
MEDIATE_METHOD(78);
MEDIATE_METHOD(79);

MEDIATE_METHOD(80);
MEDIATE_METHOD(81);
MEDIATE_METHOD(82);
MEDIATE_METHOD(83);
MEDIATE_METHOD(84);
MEDIATE_METHOD(85);
MEDIATE_METHOD(86);
MEDIATE_METHOD(87);
MEDIATE_METHOD(88);
MEDIATE_METHOD(89);

MEDIATE_METHOD(90);
MEDIATE_METHOD(91);
MEDIATE_METHOD(92);
MEDIATE_METHOD(93);
MEDIATE_METHOD(94);
MEDIATE_METHOD(95);
MEDIATE_METHOD(96);
MEDIATE_METHOD(97);
MEDIATE_METHOD(98);
MEDIATE_METHOD(99);

MEDIATE_METHOD(100);
MEDIATE_METHOD(101);
MEDIATE_METHOD(102);
MEDIATE_METHOD(103);
MEDIATE_METHOD(104);
MEDIATE_METHOD(105);
MEDIATE_METHOD(106);
MEDIATE_METHOD(107);
MEDIATE_METHOD(108);
MEDIATE_METHOD(109);

MEDIATE_METHOD(110);
MEDIATE_METHOD(111);
MEDIATE_METHOD(112);
MEDIATE_METHOD(113);
MEDIATE_METHOD(114);
MEDIATE_METHOD(115);
MEDIATE_METHOD(116);
MEDIATE_METHOD(117);
MEDIATE_METHOD(118);
MEDIATE_METHOD(119);

MEDIATE_METHOD(120);
MEDIATE_METHOD(121);
MEDIATE_METHOD(122);
MEDIATE_METHOD(123);
MEDIATE_METHOD(124);
MEDIATE_METHOD(125);
MEDIATE_METHOD(126);
MEDIATE_METHOD(127);
MEDIATE_METHOD(128);
MEDIATE_METHOD(129);

MEDIATE_METHOD(130);
MEDIATE_METHOD(131);
MEDIATE_METHOD(132);
MEDIATE_METHOD(133);
MEDIATE_METHOD(134);
MEDIATE_METHOD(135);
MEDIATE_METHOD(136);
MEDIATE_METHOD(137);
MEDIATE_METHOD(138);
MEDIATE_METHOD(139);

MEDIATE_METHOD(140);
MEDIATE_METHOD(141);
MEDIATE_METHOD(142);
MEDIATE_METHOD(143);
MEDIATE_METHOD(144);
MEDIATE_METHOD(145);
MEDIATE_METHOD(146);
MEDIATE_METHOD(147);
MEDIATE_METHOD(148);
MEDIATE_METHOD(149);

MEDIATE_METHOD(150);
MEDIATE_METHOD(151);
MEDIATE_METHOD(152);
MEDIATE_METHOD(153);
MEDIATE_METHOD(154);
MEDIATE_METHOD(155);
MEDIATE_METHOD(156);
MEDIATE_METHOD(157);
MEDIATE_METHOD(158);
MEDIATE_METHOD(159);

MEDIATE_METHOD(160);
MEDIATE_METHOD(161);
MEDIATE_METHOD(162);
MEDIATE_METHOD(163);
MEDIATE_METHOD(164);
MEDIATE_METHOD(165);
MEDIATE_METHOD(166);
MEDIATE_METHOD(167);
MEDIATE_METHOD(168);
MEDIATE_METHOD(169);

MEDIATE_METHOD(170);
MEDIATE_METHOD(171);
MEDIATE_METHOD(172);
MEDIATE_METHOD(173);
MEDIATE_METHOD(174);
MEDIATE_METHOD(175);
MEDIATE_METHOD(176);
MEDIATE_METHOD(177);
MEDIATE_METHOD(178);
MEDIATE_METHOD(179);

MEDIATE_METHOD(180);
MEDIATE_METHOD(181);
MEDIATE_METHOD(182);
MEDIATE_METHOD(183);
MEDIATE_METHOD(184);
MEDIATE_METHOD(185);
MEDIATE_METHOD(186);
MEDIATE_METHOD(187);
MEDIATE_METHOD(188);
MEDIATE_METHOD(189);

MEDIATE_METHOD(190);
MEDIATE_METHOD(191);
MEDIATE_METHOD(192);
MEDIATE_METHOD(193);
MEDIATE_METHOD(194);
MEDIATE_METHOD(195);
MEDIATE_METHOD(196);
MEDIATE_METHOD(197);
MEDIATE_METHOD(198);
MEDIATE_METHOD(199);

MEDIATE_METHOD(200);
MEDIATE_METHOD(201);
MEDIATE_METHOD(202);
MEDIATE_METHOD(203);
MEDIATE_METHOD(204);
MEDIATE_METHOD(205);
MEDIATE_METHOD(206);
MEDIATE_METHOD(207);
MEDIATE_METHOD(208);
MEDIATE_METHOD(209);

MEDIATE_METHOD(210);
MEDIATE_METHOD(211);
MEDIATE_METHOD(212);
MEDIATE_METHOD(213);
MEDIATE_METHOD(214);
MEDIATE_METHOD(215);
MEDIATE_METHOD(216);
MEDIATE_METHOD(217);
MEDIATE_METHOD(218);
MEDIATE_METHOD(219);

MEDIATE_METHOD(220);
MEDIATE_METHOD(221);
MEDIATE_METHOD(222);
MEDIATE_METHOD(223);
MEDIATE_METHOD(224);
MEDIATE_METHOD(225);
MEDIATE_METHOD(226);
MEDIATE_METHOD(227);
MEDIATE_METHOD(228);
MEDIATE_METHOD(229);

MEDIATE_METHOD(230);
MEDIATE_METHOD(231);
MEDIATE_METHOD(232);
MEDIATE_METHOD(233);
MEDIATE_METHOD(234);
MEDIATE_METHOD(235);
MEDIATE_METHOD(236);
MEDIATE_METHOD(237);
MEDIATE_METHOD(238);
MEDIATE_METHOD(239);

MEDIATE_METHOD(240);
MEDIATE_METHOD(241);
MEDIATE_METHOD(242);
MEDIATE_METHOD(243);
MEDIATE_METHOD(244);
MEDIATE_METHOD(245);
MEDIATE_METHOD(246);
MEDIATE_METHOD(247);
MEDIATE_METHOD(248);
MEDIATE_METHOD(249);

MEDIATE_METHOD(250);
MEDIATE_METHOD(251);
MEDIATE_METHOD(252);
MEDIATE_METHOD(253);
MEDIATE_METHOD(254);
MEDIATE_METHOD(255);
