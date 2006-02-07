#ifndef __ASM_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ASM_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: asm.h,v 1.16 2004/01/06 12:32:10 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: support routines for hand-written assembler
 * **************************************************************************/

// VVV

#define GLBL_LABEL(x) .globl x; x:

#define CODE(x)  x
#define C_TEXT(x)  x
#define C_DATA(x)  x

#define CODE_LABEL(x)   GLBL_LABEL(CODE(x))
#define C_TEXT_LABEL(x) GLBL_LABEL(C_TEXT(x))
#define C_DATA_LABEL(x) GLBL_LABEL(C_DATA(x))

// on i386 (and amd64 assumedly) align values are absolute values
// text and data have been aligned here 4 times
// bigger than for 32 bit XXX pdb

#define CODE_ENTRY(x) \
        .text; .align 8; GLBL_LABEL(CODE(x))

// it seems that .end directive essentially tells the assembler
// to forget the rest of the file in gas.  do not want that

#define CODE_END(x)

#define C_TEXT_END(x) \
        CODE_END(C_TEXT(x))


#define C_TEXT_ENTRY(x) \
        .text; .align 8; GLBL_LABEL(C_TEXT(x))

#define C_DATA_ENTRY(x) \
        .data; .align 16; GLBL_LABEL(C_DATA(x))

/* A function descriptor for AMD64 is just a pointer to that function */
#define FUNCDESC(label, symbol) C_DATA_ENTRY(label) .quad CODE(symbol)

/* An external entry point is just a function descriptor */
#define ENTRY_POINT_DESC(label, symbol) FUNCDESC(label, symbol)


/* in order to force relocation for data to be 64 bit we need to use
 * movabsq instead of movq which indicate 32 bit range.
 * we use gcc option mcmodel=medium to get text reloc 32 bit and data 64 bit
 * needed for us.
 */
#define LOAD_C_TEXT_ADDR(reg,symbol)\
        movabsq    $C_TEXT(symbol),reg

#define LOAD_C_DATA_ADDR(reg,symbol)\
        movabsq    $C_DATA(symbol),reg

#define LOAD_C_DATA_OFFSET(reg,symbol,offset)				\
        movabsq 	 $(C_DATA(symbol)),reg;				\
	leaq	offset(reg), reg

#define LOAD_C_DATA_UVAL(reg,symbol,offset)				\
        movabsq    $C_DATA(symbol),reg;					\
	movq	offset(reg), reg;


#define STORE_C_DATA_UVAL(reg,symbol,offset)\
        movabsq    reg,(C_DATA(symbol)+(offset))


/* we assume running gcc -fomit-frame-pointer with NDEBUG, otherwise
 * we compile with frame pointer to facilitate debugging (and profiling).
 * Compensate for the 16 byte alignment w/ dummy pushq. XXX
 */
#ifdef NDEBUG
#define DEBUG_PUSH_FRAME1()
#define DEBUG_PUSH_FRAME2()
#define DEBUG_POP_FRAME1()
#define DEBUG_POP_FRAME2()
#else /* #ifdef NDEBUG */
#define DEBUG_PUSH_FRAME1()	\
        pushq   %rbp;
#define DEBUG_POP_FRAME1() 	\
        popq    %rbp;
#define DEBUG_PUSH_FRAME2()	\
        movq    %rsp,%rbp;
#define DEBUG_POP_FRAME2() 	\
        movq    %rbp,%rsp;
#endif /* #ifdef NDEBUG */


// The following may or may not push %rbp (depending on whether we
// need it for a stack trace back).  The ENTER matches the REMOVE.
// The RETURN both REMOVES and returns.

#define LEAF_ENTER()		\
        DEBUG_PUSH_FRAME1()

#define LEAF_REMOVE()		\
        DEBUG_POP_FRAME1()	\

#define LEAF_RETURN()		\
        LEAF_REMOVE()		\
        ret


// The following may or may not push %rbp (as above),
// and also updates %rbp to save the current stack pointer.
// (for a stack trace back).  The ENTER matches the REMOVE.
// The RETURN both REMOVES and returns.


#define FRAME_ENTER()		\
        LEAF_ENTER()		\
        DEBUG_PUSH_FRAME2()

#define FRAME_REMOVE()		\
        DEBUG_POP_FRAME2()	\
        LEAF_REMOVE()

#define FRAME_RETURN()		\
	FRAME_REMOVE()		\
	ret


/* FULLSAVE_FRAME_ENTER() saves non volatile gpr's
 */
#define FULLSAVE_FRAME_ENTER()	\
        FRAME_ENTER();		\
	pushq	%rbx;		\
	pushq	%rbp;		\
	pushq	%r12;		\
	pushq	%r13;		\
	pushq	%r14;		\
	pushq	%r15;

#define FULLSAVE_FRAME_RETURN()	\
	popq	%r15;		\
	popq	%r14;		\
	popq	%r13;		\
	popq	%r12;		\
	popq	%rbp;		\
	popq	%rbx;		\
        FRAME_RETURN()

// generate 16 bytes in proper byte-order	what is that for ? XXX
#define HEX_BYTES_16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
        .octa 0x##p##o##n##m##l##k##j##i##h##g##f##e##d##c##b##a

// WARNING:  This macro must be kept consistent with _SERROR in SysStatus.H.
#define SERROR(reg,ec,cc,gc) \
	movq	$(1<<63) | ((ec)<<16) | ((cc)<<8) | (gc), reg



