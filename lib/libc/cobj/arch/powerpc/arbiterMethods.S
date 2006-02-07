/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: arbiterMethods.S,v 1.1 2004/01/24 20:58:16 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

/*
 * author: Raymond Fingas
 * based on interposerMethods.S by Craig Soules
 */

#include <sys/kinclude.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

// define the offsets within the arbiter object
#define COBJ_VTABLE       (0)
#define COBJ_myRoot       (8)  // from CObjRep
#define COBJ_rep          (16) // from CObjRepArbiterTarget
#define COBJ_arbitrater   (24) // from CObjRepArbiterTarget
// FIXME make macros automagically calculate correct stack number?
// these should go in genConstants, but that would have to includes
// asmConstants.H, which is circular and won't compile--the correct
// solution is to remove the dependency on asmConstants.H (in the C
// arch-independent source file!) but I'm too lazy, err, I want to
// finish this year.
// The correct solution is likely a libraries asmConstants file.
#define COBJ_tempStacks     (32) // from CObjRepArbiterTarget
//#define COBJ_stackOffset  (40) // from CObjRepArbiterTarget
//#define COBJ_stackBase    (48) // from CObjRepArbiterTarget
//#define COBJ_bottomSP     (56) // from CObjRepArbiterTarget
//#define COBJ_truebottomSP (64) // from CObjRepArbiterTarget

#define TS_stackOffset      (0x00) // from TempStack
#define TS_stackBase        (0x08) // from TempStack
#define TS_bottomSP         (0x10) // from TempStack
#define TS_truebottomSP     (0x18) // from TempStack
#define TS_next             (0x20) // from TempStack
#define TS_recurCount       (0x28) // from TempStack

#define CORC_targetRep    (16)  // from CObjRepArbiter

// define the non-volatile register being used
// supplanted by thread specific stack pointer in thread descriptor
//#define NVREG r28

CODE_ENTRY(arbiterMethodCommon)
// offsets for variables relative to stack (or CallDescriptor)
//#define startSP_POS             ((-14)*8)
#define CORCT_PTR_POS           ((-14)*8)
#define bottomSP_POS            ((-15)*8)
#define truebottomSP_POS        ((-16)*8)
#define TOTAL_ADJUSTMENT        (17*8)

// save code
	// by abi, r1=sp, r2=toc, r13=&CurrentThread[reserved, but in K42]
	// initially r3=this, and r4-r10 hold the other params
        // r11 and r12 are available (but r12 is our fnum parameter)
        // r13-r31 are nonvolatile

	mr      r0,r6                           // backup r6 so we can use it
                                                // for temp addy reg
// we need to get a TempStack here
// First check r13, and if that comes up blank, get one from the arbiter free
// list. We can't make a function call here because that would screw up the
// volatiles, and we don't want to save them multiple times
        ld      r6,TH_altStack(r13)
        cmpldi  r6,0
        bne-    $+32	// recursive--already have a stack for this thread
        // note that this isn't necessarily recursion--if we use thread-
        // specific stacks, then multiple arbiters could use the same stack

        // LL/SC tempStacks
        // If we overwrote it then we get the stack we removed from the free
        // list, but if someone else overwrote it, then try again.
	addi	r3,r3,COBJ_tempStacks  // LL uses register indexed only
        ldarx	r6,0,r3
	tdi	4,r6,0      // trap if no temporary stack available
	ld	r11,TS_next(r6) // tempStacks = tempStacks->next
	stdcx.  r11,0,r3
	bne-    $-12	// got stack?
	addi	r3,r3,-COBJ_tempStacks          // restore this pointer

	std     r6,TH_altStack(r13)

        ld      r11,TS_recurCount(r6)   // update recursion count
        addi    r11,r11,1
        std     r11,TS_recurCount(r6)

	ld      r11,TS_stackOffset(r6)


        std     r3,CORCT_PTR_POS(r11)            // save this for callDescriptor
   
	// fix bottomSP and startSP
#ifndef NDEBUG
// move value in thread to stack, then from arbiterTarget to thread
        ld      r3,TH_bottomSP(r13)             // replace bottomSP
        std     r3,(bottomSP_POS)(r11)
// The 0'ing of bottomSP should prevent checks on the stack when it is in an
// inconsistent state w.r.t. error checking.
	addi	r3,0,0
	std	r3,TH_bottomSP(r13)		// 0 bottomSP to bypass checks
        ld      r3,TH_truebottomSP(r13)         // replace truebottomSP
        std     r3,(truebottomSP_POS)(r11)
        ld      r3,TS_truebottomSP(r6)
        std     r3,TH_truebottomSP(r13)
#endif
        
        // swing stack to temp
        std     r1,0(r11)                       // save original stack pointer
        mr      r1,r11                          // r1 = tempStack

#ifndef NDEBUG
        ld      r3,TS_bottomSP(r6)
        std     r3,TH_bottomSP(r13)
        ld      r3,CORCT_PTR_POS(r11)            // restore r3 for later
#endif

	mflr	r6				// move link register to r6
                                                // r1 is stack register
	std	r6,STK_LR(r1)                   // store link register
        // FIXME maybe should be stored in LR slot on old stack?
        // This may confuse the debugger, since it sees a wrong LR
        // don't store r3--it's the wrong this pointer for original call!
	//std	r3,STK_PARAM0(r1)		// save parameter gprs
	std	r4,STK_PARAM1(r1)               // at higher address than stack
	std	r5,STK_PARAM2(r1)               // according to convention
        std     r0,STK_PARAM3(r1) // moved r6 to r0 earlier
	std	r7,STK_PARAM4(r1)
	std	r8,STK_PARAM5(r1)
	std	r9,STK_PARAM6(r1)
	std	r10,STK_PARAM7(r1)
	stfd	f1,((-1)*8)(r1)			// save parameter fprs
	stfd	f2,((-2)*8)(r1)
	stfd	f3,((-3)*8)(r1)
	stfd	f4,((-4)*8)(r1)
	stfd	f5,((-5)*8)(r1)
	stfd	f6,((-6)*8)(r1)
	stfd	f7,((-7)*8)(r1)
	stfd	f8,((-8)*8)(r1)
	stfd	f9,((-9)*8)(r1)
	stfd	f10,((-10)*8)(r1)
	stfd	f11,((-11)*8)(r1)
	stfd	f12,((-12)*8)(r1)
	stfd	f13,((-13)*8)(r1)

// new setup code
// This is assembly equivalent of DREF(), so if anything changes that changes
// DREF, this code has to change to reflect the new mechanism as well.
	mr	r7, r3
	ld	r3, COBJ_arbitrater(r7)        // set this for arbitrater
	ld      r3, 0(r3)                       // ref needs extra indirection

	ld	r6, 0(r3)			// r6 = arbitrater vtable
	// FIXME perhaps a static C function could make this call instead
        // this seems to be one of the most likely places for breakage, in
        // particular if BaseObj or CObjRep changes
	ld	r6, (112)(r6)		      // r6 = arbitrater->handleCall()
	ld	r0, 0(r6)
	ld	r2, 8(r6)			// load TOC
	mr	r4, r1				// param1 = call descriptor ptr
	mr	r5, r12				// param2 = f'n number

	// FIXME make sure this coincides with the object it claims to be
	// which is definitely not the case now (no vtable ptr saved!)
	addi	r4, r4,-(TOTAL_ADJUSTMENT-8)   // adjust for params saved

	mtctr	r0
	stdu	r1, -(STK_SIZE+(TOTAL_ADJUSTMENT))(r1)	// push stack frame
	bctrl					// call arbitrater->handleCall()

	// handleCall() made the actual function call (or not... whatever)
	// we just need to swap the stack back and return, being careful to
	// preserve the return values (r3 and r4, plus some fp registers)
	
	la      r1, (STK_SIZE+(TOTAL_ADJUSTMENT))(r1)	// pop stack frame

	ld      r0, STK_LR(r1)			// restore link register
	mtlr    r0

        ld      r8,CORCT_PTR_POS(r1)            // need this for stack return
#ifndef NDEBUG
// move value on stack to thread
	addi	r0,0,0
	std	r0,TH_bottomSP(r13)		// 0 bottomSP to bypass checks
        ld      r0,(truebottomSP_POS)(r1)
        std     r0,TH_truebottomSP(r13)         // replace truebottomSP
        ld      r0,(bottomSP_POS)(r1)
#endif
        //ld      r0,(startSP_POS)(r1)
        //std     r0,TH_startSP(r13)              // replace startSP
        //std     r6,TH_startSP(r13)

        ld      r1, 0(r1)                       // load original stack pointer

// and unzero it here to get the checks back
#ifndef NDEBUG
        std     r0,TH_bottomSP(r13)             // replace bottomSP
#endif

        // reduce recursion count
        ld      r7,TH_altStack(r13)
        ld      r6,TS_recurCount(r7)
        addi    r6,r6,(-1)
        std     r6,TS_recurCount(r7)
        cmpldi  r6,0

        // if it's 0, return the stack
        // If we expected to use the same stack on multiple processors
        // (obviously just one at a time (= ) we'd have to sync here to make
        // sure that we weren't left with it in altStack and on the list.
        bne-    $+28
        std     r6,TH_altStack(r13)    // zero thread altStack (r6=0 here)
        addi    r8,r8,COBJ_tempStacks  // r8=&(COBJ->tempStacks)
        ldarx   r6,0,r8                // r6=COBJ->tempStacks
        std     r6,TS_next(r7)         // curStack->next=COBJ->tempStacks
        stdcx.  r7,0,r8                // COBJ->tempStacks=curStack
        bne-    $-12
 
// FIXME we have to give back the stack here if the recursion count is zero
	blr					// return!

CODE_END(arbiterMethodCommon)

C_TEXT_BASIC_ENTRY(arbiterCallOriginalMethod)
// r3=this (CORepArbiter), r4=CallDescriptor*, r5=funcPtr
	mflr	r0
	std	r0,STK_LR(r1)
	addi	r4,r4,(TOTAL_ADJUSTMENT-8)      // adjust for params saved

	// restore floating point params
	lfd f1,((-1)*8)(r4)
	lfd f2,((-2)*8)(r4)
	lfd f3,((-3)*8)(r4)
	lfd f4,((-4)*8)(r4)
	lfd f5,((-5)*8)(r4)
	lfd f6,((-6)*8)(r4)
	lfd f7,((-7)*8)(r4)
	lfd f8,((-8)*8)(r4)
	lfd f9,((-9)*8)(r4)
	lfd f10,((-10)*8)(r4)
	lfd f11,((-11)*8)(r4)
	lfd f12,((-12)*8)(r4)
	lfd f13,((-13)*8)(r4)

	ld	r7,TH_altStack(r13)     // get stack data
// store SP in TempStack
        ld      r0,TS_stackOffset(r7) // save original stackOffset on stack
        std     r0,((-2)*8)(r1)
        stdu    r1,-(STK_SIZE+(3*8))(r1) // save new stackOffset in target,
        std     r1,TS_stackOffset(r7) // updating frame in case of recursion
        
// restore stack
#ifndef NDEBUG
	addi	r0,0,0
	std	r0,TH_bottomSP(r13)		// 0 bottomSP to bypass checks
        ld      r0,(truebottomSP_POS)(r4)
        std     r0,TH_truebottomSP(r13)         // replace truebottomSP
#endif
        //ld      r0,(startSP_POS)(r4)
        //std     r0,TH_startSP(r13)
        ld      r1,0(r4)                        // load original stack pointer

#ifndef NDEBUG
        ld      r0,(bottomSP_POS)(r4)
        std     r0,TH_bottomSP(r13)             // replace bottomSP
#endif

// get f'n pointer
        ld      r3,CORC_targetRep(r3)		// set new this pointer for call
	ld	r2,8(r5)			// load TOC
        ld      r0,0(r5)
        mtctr   r0

// restore integer params
        ld      r10,STK_PARAM7(r4)
        ld      r9,STK_PARAM6(r4)
        ld      r8,STK_PARAM5(r4)
        ld      r7,STK_PARAM4(r4)
        ld      r6,STK_PARAM3(r4)
        ld      r5,STK_PARAM2(r4)
        ld      r4,STK_PARAM1(r4)
        
// call
        bctrl

// undo the damage
	ld	r7,TH_altStack(r13)
#ifndef NDEBUG
	addi	r0,0,0
	std	r0,TH_bottomSP(r13)		// 0 bottomSP to bypass checks
        ld      r0,TS_truebottomSP(r7)
        std     r0,TH_truebottomSP(r13)
#endif
        ld      r1,TS_stackOffset(r7)// restore stack pointer
        la      r1,(STK_SIZE+(3*8))(r1)

#ifndef NDEBUG
        ld      r0,TS_bottomSP(r7)
        std     r0,TH_bottomSP(r13)
#endif

        ld      r0,((-2)*8)(r1)         // restore stackOffset
        std     r0,TS_stackOffset(r7)

	addi	r8,0,0
	//std	r8,TH_altStack(r13)

        ld      r0,STK_LR(r1)
        mtlr    r0
        blr

C_TEXT_BASIC_END(arbiterCallOriginalMethod)

#define ARBITER_METHOD(OP)\
    C_TEXT_BASIC_ENTRY(arbiterMethod ## OP);\
	li	r12,(OP);\
	b	arbiterMethodCommon;\
    C_TEXT_BASIC_END(arbiterMethod ## OP)

// method definitions

ARBITER_METHOD(0) // not sure we want this as first vtable seems
		// empty or at least reserved for other uses, but for the sake
		// of symmetry
		// this note brought to you by mediateMethods.S
ARBITER_METHOD(1)
ARBITER_METHOD(2)
ARBITER_METHOD(3)
ARBITER_METHOD(4)
ARBITER_METHOD(5)
ARBITER_METHOD(6)
ARBITER_METHOD(7)
ARBITER_METHOD(8)
ARBITER_METHOD(9)

ARBITER_METHOD(10);
ARBITER_METHOD(11);
ARBITER_METHOD(12);
ARBITER_METHOD(13);
ARBITER_METHOD(14);
ARBITER_METHOD(15);
ARBITER_METHOD(16);
ARBITER_METHOD(17);
ARBITER_METHOD(18);
ARBITER_METHOD(19);

ARBITER_METHOD(20);
ARBITER_METHOD(21);
ARBITER_METHOD(22);
ARBITER_METHOD(23);
ARBITER_METHOD(24);
ARBITER_METHOD(25);
ARBITER_METHOD(26);
ARBITER_METHOD(27);
ARBITER_METHOD(28);
ARBITER_METHOD(29);

ARBITER_METHOD(30);
ARBITER_METHOD(31);
ARBITER_METHOD(32);
ARBITER_METHOD(33);
ARBITER_METHOD(34);
ARBITER_METHOD(35);
ARBITER_METHOD(36);
ARBITER_METHOD(37);
ARBITER_METHOD(38);
ARBITER_METHOD(39);

ARBITER_METHOD(40);
ARBITER_METHOD(41);
ARBITER_METHOD(42);
ARBITER_METHOD(43);
ARBITER_METHOD(44);
ARBITER_METHOD(45);
ARBITER_METHOD(46);
ARBITER_METHOD(47);
ARBITER_METHOD(48);
ARBITER_METHOD(49);

ARBITER_METHOD(50);
ARBITER_METHOD(51);
ARBITER_METHOD(52);
ARBITER_METHOD(53);
ARBITER_METHOD(54);
ARBITER_METHOD(55);
ARBITER_METHOD(56);
ARBITER_METHOD(57);
ARBITER_METHOD(58);
ARBITER_METHOD(59);

ARBITER_METHOD(60);
ARBITER_METHOD(61);
ARBITER_METHOD(62);
ARBITER_METHOD(63);
ARBITER_METHOD(64);
ARBITER_METHOD(65);
ARBITER_METHOD(66);
ARBITER_METHOD(67);
ARBITER_METHOD(68);
ARBITER_METHOD(69);

ARBITER_METHOD(70);
ARBITER_METHOD(71);
ARBITER_METHOD(72);
ARBITER_METHOD(73);
ARBITER_METHOD(74);
ARBITER_METHOD(75);
ARBITER_METHOD(76);
ARBITER_METHOD(77);
ARBITER_METHOD(78);
ARBITER_METHOD(79);

ARBITER_METHOD(80);
ARBITER_METHOD(81);
ARBITER_METHOD(82);
ARBITER_METHOD(83);
ARBITER_METHOD(84);
ARBITER_METHOD(85);
ARBITER_METHOD(86);
ARBITER_METHOD(87);
ARBITER_METHOD(88);
ARBITER_METHOD(89);

ARBITER_METHOD(90);
ARBITER_METHOD(91);
ARBITER_METHOD(92);
ARBITER_METHOD(93);
ARBITER_METHOD(94);
ARBITER_METHOD(95);
ARBITER_METHOD(96);
ARBITER_METHOD(97);
ARBITER_METHOD(98);
ARBITER_METHOD(99);

ARBITER_METHOD(100);
ARBITER_METHOD(101);
ARBITER_METHOD(102);
ARBITER_METHOD(103);
ARBITER_METHOD(104);
ARBITER_METHOD(105);
ARBITER_METHOD(106);
ARBITER_METHOD(107);
ARBITER_METHOD(108);
ARBITER_METHOD(109);

ARBITER_METHOD(110);
ARBITER_METHOD(111);
ARBITER_METHOD(112);
ARBITER_METHOD(113);
ARBITER_METHOD(114);
ARBITER_METHOD(115);
ARBITER_METHOD(116);
ARBITER_METHOD(117);
ARBITER_METHOD(118);
ARBITER_METHOD(119);

ARBITER_METHOD(120);
ARBITER_METHOD(121);
ARBITER_METHOD(122);
ARBITER_METHOD(123);
ARBITER_METHOD(124);
ARBITER_METHOD(125);
ARBITER_METHOD(126);
ARBITER_METHOD(127);
ARBITER_METHOD(128);
ARBITER_METHOD(129);

ARBITER_METHOD(130);
ARBITER_METHOD(131);
ARBITER_METHOD(132);
ARBITER_METHOD(133);
ARBITER_METHOD(134);
ARBITER_METHOD(135);
ARBITER_METHOD(136);
ARBITER_METHOD(137);
ARBITER_METHOD(138);
ARBITER_METHOD(139);

ARBITER_METHOD(140);
ARBITER_METHOD(141);
ARBITER_METHOD(142);
ARBITER_METHOD(143);
ARBITER_METHOD(144);
ARBITER_METHOD(145);
ARBITER_METHOD(146);
ARBITER_METHOD(147);
ARBITER_METHOD(148);
ARBITER_METHOD(149);

ARBITER_METHOD(150);
ARBITER_METHOD(151);
ARBITER_METHOD(152);
ARBITER_METHOD(153);
ARBITER_METHOD(154);
ARBITER_METHOD(155);
ARBITER_METHOD(156);
ARBITER_METHOD(157);
ARBITER_METHOD(158);
ARBITER_METHOD(159);

ARBITER_METHOD(160);
ARBITER_METHOD(161);
ARBITER_METHOD(162);
ARBITER_METHOD(163);
ARBITER_METHOD(164);
ARBITER_METHOD(165);
ARBITER_METHOD(166);
ARBITER_METHOD(167);
ARBITER_METHOD(168);
ARBITER_METHOD(169);

ARBITER_METHOD(170);
ARBITER_METHOD(171);
ARBITER_METHOD(172);
ARBITER_METHOD(173);
ARBITER_METHOD(174);
ARBITER_METHOD(175);
ARBITER_METHOD(176);
ARBITER_METHOD(177);
ARBITER_METHOD(178);
ARBITER_METHOD(179);

ARBITER_METHOD(180);
ARBITER_METHOD(181);
ARBITER_METHOD(182);
ARBITER_METHOD(183);
ARBITER_METHOD(184);
ARBITER_METHOD(185);
ARBITER_METHOD(186);
ARBITER_METHOD(187);
ARBITER_METHOD(188);
ARBITER_METHOD(189);

ARBITER_METHOD(190);
ARBITER_METHOD(191);
ARBITER_METHOD(192);
ARBITER_METHOD(193);
ARBITER_METHOD(194);
ARBITER_METHOD(195);
ARBITER_METHOD(196);
ARBITER_METHOD(197);
ARBITER_METHOD(198);
ARBITER_METHOD(199);

ARBITER_METHOD(200);
ARBITER_METHOD(201);
ARBITER_METHOD(202);
ARBITER_METHOD(203);
ARBITER_METHOD(204);
ARBITER_METHOD(205);
ARBITER_METHOD(206);
ARBITER_METHOD(207);
ARBITER_METHOD(208);
ARBITER_METHOD(209);

ARBITER_METHOD(210);
ARBITER_METHOD(211);
ARBITER_METHOD(212);
ARBITER_METHOD(213);
ARBITER_METHOD(214);
ARBITER_METHOD(215);
ARBITER_METHOD(216);
ARBITER_METHOD(217);
ARBITER_METHOD(218);
ARBITER_METHOD(219);

ARBITER_METHOD(220);
ARBITER_METHOD(221);
ARBITER_METHOD(222);
ARBITER_METHOD(223);
ARBITER_METHOD(224);
ARBITER_METHOD(225);
ARBITER_METHOD(226);
ARBITER_METHOD(227);
ARBITER_METHOD(228);
ARBITER_METHOD(229);

ARBITER_METHOD(230);
ARBITER_METHOD(231);
ARBITER_METHOD(232);
ARBITER_METHOD(233);
ARBITER_METHOD(234);
ARBITER_METHOD(235);
ARBITER_METHOD(236);
ARBITER_METHOD(237);
ARBITER_METHOD(238);
ARBITER_METHOD(239);

ARBITER_METHOD(240);
ARBITER_METHOD(241);
ARBITER_METHOD(242);
ARBITER_METHOD(243);
ARBITER_METHOD(244);
ARBITER_METHOD(245);
ARBITER_METHOD(246);
ARBITER_METHOD(247);
ARBITER_METHOD(248);
ARBITER_METHOD(249);

ARBITER_METHOD(250);
ARBITER_METHOD(251);
ARBITER_METHOD(252);
ARBITER_METHOD(253);
ARBITER_METHOD(254);
ARBITER_METHOD(255);
