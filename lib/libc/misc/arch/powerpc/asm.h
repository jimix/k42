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
 * $Id: asm.h,v 1.41 2004/06/28 17:01:25 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: support routines for hand-written assembler
 * **************************************************************************/

#define STRINGIFY(x) #x

#define GLBL_LABEL(x) .globl x; x:

#ifdef __GNU_AS__
#define FUNCDESC(x,f) \
	 .section ".opd","aw"; .align 3; GLBL_LABEL(x) .quad f,.TOC.@tocbase, 0; .previous; .size x,24;
#else	/* #ifdef __GNU_AS__ */
#define FUNCDESC(x,f) \
	.toc; .csect x##[DS],3; GLBL_LABEL(x) .llong f,TOC[tc0],0;
#endif	/* #ifdef __GNU_AS__ */

#define ENTRY_POINT_DESC(label, symbol) FUNCDESC(label, CODE(symbol))

#define CODE(x) x
#define C_TEXT(x) . ## x
#define C_DATA(x) x

#define CODE_LABEL(x)   GLBL_LABEL(CODE(x))
#define C_TEXT_LABEL(x) GLBL_LABEL(C_TEXT(x))
#define C_DATA_LABEL(x) GLBL_LABEL(C_DATA(x))

#ifdef __GNU_AS__
/* FIXME: I believe that this .align is useless -JX */
#define TEXT_ALIGN(x)	\
	.text; .align (x)
#define TEXT_TYPE_DECL(x)  \
	.type x,@function
#else
#define TEXT_TYPE_DECL(x)
#define	TEXT_ALIGN(x)	\
	.csect .text[PR],(x)
#endif

#define BASIC_ENTRY(x)	\
	TEXT_ALIGN(2) ; GLBL_LABEL(x); TEXT_TYPE_DECL(x)
#define BASIC_END(x)


// FIXME: this extra mangling the '-' works aroung bugs in gcc 2 and 3 -JX
#ifdef __GNU_AS__
#define BEGIN_LINES(x)
#define END_LINES(x)
#else	/* #ifdef __GNU_AS__ */
#define BEGIN_LINES(x) \
	.stabx	STRINGIFY(x:F-1),x,142,0; \
	.function x,x,16,44,FE.._ ## x - x
#define END_LINES(x) \
	FE.._ ## x :
#endif	/* #ifdef __GNU_AS__ */

#define CODE_BASIC_ENTRY(x) \
	BASIC_ENTRY(CODE(x))

#define CODE_BASIC_END(x) \
	BASIC_END(CODE(x))

#define CODE_ENTRY(x) \
	BASIC_ENTRY(CODE(x)); \
	BEGIN_LINES(x) /* see NOTE below */

#define CODE_END(x) \
	END_LINES(x) /* see NOTE below */ \
	BASIC_END(CODE(x))

#define C_TEXT_BASIC_ENTRY(x) \
	FUNCDESC(x, C_TEXT(x)); \
	BASIC_ENTRY(C_TEXT(x))

#define C_TEXT_BASIC_END(x) \
	BASIC_END(C_TEXT(x))

#define C_TEXT_ENTRY(x) \
	FUNCDESC(x, C_TEXT(x)); \
	BASIC_ENTRY(C_TEXT(x)); \
	BEGIN_LINES(. ## x) /* see NOTE below */

#define C_TEXT_END(x) \
	END_LINES(. ## x) /* see NOTE below */ \
	BASIC_END(C_TEXT(x))

/*
 * NOTE:  We can't use C_TEXT(x) or CODE(x) as the argument to BEGIN_LINES or
 *        END_LINES because the GCC preprocessor wraps white space around the
 *        result of a macro expansion, preventing further concatenation.
 */

#ifdef	__GNU_AS__
#define C_DATA_ENTRY(x) \
	.section ".data"; .align 3; GLBL_LABEL(C_DATA(x))
#define C_DATA_ENTRY_ALIGNED(x,alignment) \
	.section ".data"; .align alignment; GLBL_LABEL(C_DATA(x))
#else	/*  __GNU_AS__ */
#define C_DATA_ENTRY(x) \
	.csect .data[RW],3; GLBL_LABEL(C_DATA(x))
#define C_DATA_ENTRY_ALIGNED(x,alignment) \
	.csect .data[RW],alignment; GLBL_LABEL(C_DATA(x))
#endif	/* __GNU_AS__ */

#define TOC_CODE_SYM(symbol) symbol##.TE
#define TOC_C_TEXT_SYM(symbol) .##symbol##.TE
#define TOC_C_DATA_SYM(symbol) symbol##.TE

#ifdef __GNU_AS__
#define TOC_CODE_ENTRY(symbol) \
	.section ".toc","aw"; TOC_CODE_SYM(symbol): .tc symbol##[TC],symbol; .previous

#define TOC_C_TEXT_ENTRY(symbol) \
	.section ".toc","aw"; TOC_C_TEXT_SYM(symbol): .tc .##symbol##[TC],.##symbol; .previous

#define TOC_C_DATA_ENTRY(symbol) \
	.section ".toc","aw"; TOC_C_DATA_SYM(symbol): .tc symbol##[TC],symbol; .previous
#else
#define TOC_CODE_ENTRY(symbol) \
	.toc; TOC_CODE_SYM(symbol): .tc symbol##[TC],symbol

#define TOC_C_TEXT_ENTRY(symbol) \
	.toc; TOC_C_TEXT_SYM(symbol): .tc .##symbol##[TC],.##symbol

#define TOC_C_DATA_ENTRY(symbol) \
	.toc; TOC_C_DATA_SYM(symbol): .tc symbol##[TC],symbol
#endif

#define LOAD_CODE_ADDR(reg,symbol)\
	ld	reg,TOC_CODE_SYM(symbol)(r2)

#define LOAD_C_TEXT_ADDR(reg,symbol)\
	ld	reg,TOC_C_TEXT_SYM(symbol)(r2)

#define LOAD_C_DATA_ADDR(reg,symbol)\
	ld	reg,TOC_C_DATA_SYM(symbol)(r2)

#define LOAD_C_DATA_OFFSET(reg,symbol,offset)\
	LOAD_C_DATA_ADDR(reg,symbol);\
	la	reg,(offset)(reg)

#define LOAD_C_DATA_UVAL(reg,symbol,offset)\
	LOAD_C_DATA_ADDR(reg,symbol);\
	ld	reg,(offset)(reg)

#define STORE_C_DATA_UVAL(reg,symbol,offset,scratchReg)\
	LOAD_C_DATA_ADDR(scratchReg,symbol);\
	std	reg,(offset)(scratchReg)

	/*
	 * Stack frame offsets
	 */

	/*
	 * TOC calling conventions let a procedure store all the non-volatile
	 * registers beyond the end of the stack without moving the stack
	 * pointer.  Interrupts and traps must preserve this area.  STK_FLOOR
	 * is the lowest offset from the stack pointer that must not be
	 * trampled.  Its magnitude is (19*8 + 18*8) because there are 19
	 * non-volatile GPR's and 18 non-volatile FPR's.
	 */
#define	STK_FLOOR	(-0x128)

#define	STK_BACKCHAIN	0x0
#define	STK_CR		0x8
#define	STK_LR		0x10
#define	STK_COMPILER	0x18
#define	STK_BINDER	0x20
#define	STK_TOC		0x28
#define	STK_PARAM0	0x30
#define	STK_PARAM1	0x38
#define	STK_PARAM2	0x40
#define	STK_PARAM3	0x48
#define	STK_PARAM4	0x50
#define	STK_PARAM5	0x58
#define	STK_PARAM6	0x60
#define	STK_PARAM7	0x68

#define	STK_SIZE	0x70

#define	STK_LOCAL0	0x70
#define	STK_LOCAL1	0x78
#define	STK_LOCAL2	0x80
#define	STK_LOCAL3	0x88
#define	STK_LOCAL4	0x90
#define	STK_LOCAL5	0x98

#define LEAF_ENTER(scratchReg)\
	mflr	scratchReg;\
	std	scratchReg,STK_LR(r1)

#define LEAF_RETURN(scratchReg)\
	ld	scratchReg,STK_LR(r1);\
	mtlr	scratchReg;\
	blr

#define FRAME_ENTER(reservation, scratchReg)\
	mflr	scratchReg;\
	std	scratchReg,STK_LR(r1);\
	stdu	r1,-(STK_SIZE+((reservation)*8))(r1)

#define FRAME_RETURN(reservation, scratchReg)\
	ld	scratchReg,((STK_SIZE+((reservation)*8))+STK_LR)(r1);\
	mtlr	scratchReg;\
	la	r1,(STK_SIZE+((reservation)*8))(r1);\
	blr

#ifndef __ASSEMBLER__
struct NonvolatileState {
    uval64 r14,r15,r16,r17,r18,r19,r20,r21,r22,
	   r23,r24,r25,r26,r27,r28,r29,r30,r31;
    uval64 f14,f15,f16,f17,f18,f19,f20,f21,f22,
	   f23,f24,f25,f26,f27,f28,f29,f30,f31;
    
    void init() { memset(this, 0, sizeof(*this)); }
};
#endif // !__ASSEMBLER__

#define NVS_SIZE ((18+18)*8)
#define NVS_r(n) ((0  + ((n)-14))*8)
#define NVS_f(n) ((18 + ((n)-14))*8)

#define NVS_SAVE(nvsBaseReg,nvsOffset)\
	std	r14,(nvsOffset)+NVS_r(14)(nvsBaseReg);\
	std	r15,(nvsOffset)+NVS_r(15)(nvsBaseReg);\
	std	r16,(nvsOffset)+NVS_r(16)(nvsBaseReg);\
	std	r17,(nvsOffset)+NVS_r(17)(nvsBaseReg);\
	std	r18,(nvsOffset)+NVS_r(18)(nvsBaseReg);\
	std	r19,(nvsOffset)+NVS_r(19)(nvsBaseReg);\
	std	r20,(nvsOffset)+NVS_r(20)(nvsBaseReg);\
	std	r21,(nvsOffset)+NVS_r(21)(nvsBaseReg);\
	std	r22,(nvsOffset)+NVS_r(22)(nvsBaseReg);\
	std	r23,(nvsOffset)+NVS_r(23)(nvsBaseReg);\
	std	r24,(nvsOffset)+NVS_r(24)(nvsBaseReg);\
	std	r25,(nvsOffset)+NVS_r(25)(nvsBaseReg);\
	std	r26,(nvsOffset)+NVS_r(26)(nvsBaseReg);\
	std	r27,(nvsOffset)+NVS_r(27)(nvsBaseReg);\
	std	r28,(nvsOffset)+NVS_r(28)(nvsBaseReg);\
	std	r29,(nvsOffset)+NVS_r(29)(nvsBaseReg);\
	std	r30,(nvsOffset)+NVS_r(30)(nvsBaseReg);\
	std	r31,(nvsOffset)+NVS_r(31)(nvsBaseReg);\
	stfd	f14,(nvsOffset)+NVS_f(14)(nvsBaseReg);\
	stfd	f15,(nvsOffset)+NVS_f(15)(nvsBaseReg);\
	stfd	f16,(nvsOffset)+NVS_f(16)(nvsBaseReg);\
	stfd	f17,(nvsOffset)+NVS_f(17)(nvsBaseReg);\
	stfd	f18,(nvsOffset)+NVS_f(18)(nvsBaseReg);\
	stfd	f19,(nvsOffset)+NVS_f(19)(nvsBaseReg);\
	stfd	f20,(nvsOffset)+NVS_f(20)(nvsBaseReg);\
	stfd	f21,(nvsOffset)+NVS_f(21)(nvsBaseReg);\
	stfd	f22,(nvsOffset)+NVS_f(22)(nvsBaseReg);\
	stfd	f23,(nvsOffset)+NVS_f(23)(nvsBaseReg);\
	stfd	f24,(nvsOffset)+NVS_f(24)(nvsBaseReg);\
	stfd	f25,(nvsOffset)+NVS_f(25)(nvsBaseReg);\
	stfd	f26,(nvsOffset)+NVS_f(26)(nvsBaseReg);\
	stfd	f27,(nvsOffset)+NVS_f(27)(nvsBaseReg);\
	stfd	f28,(nvsOffset)+NVS_f(28)(nvsBaseReg);\
	stfd	f29,(nvsOffset)+NVS_f(29)(nvsBaseReg);\
	stfd	f30,(nvsOffset)+NVS_f(30)(nvsBaseReg);\
	stfd	f31,(nvsOffset)+NVS_f(31)(nvsBaseReg)

#define NVS_RESTORE(nvsBaseReg,nvsOffset)\
	ld	r14,(nvsOffset)+NVS_r(14)(nvsBaseReg);\
	ld	r15,(nvsOffset)+NVS_r(15)(nvsBaseReg);\
	ld	r16,(nvsOffset)+NVS_r(16)(nvsBaseReg);\
	ld	r17,(nvsOffset)+NVS_r(17)(nvsBaseReg);\
	ld	r18,(nvsOffset)+NVS_r(18)(nvsBaseReg);\
	ld	r19,(nvsOffset)+NVS_r(19)(nvsBaseReg);\
	ld	r20,(nvsOffset)+NVS_r(20)(nvsBaseReg);\
	ld	r21,(nvsOffset)+NVS_r(21)(nvsBaseReg);\
	ld	r22,(nvsOffset)+NVS_r(22)(nvsBaseReg);\
	ld	r23,(nvsOffset)+NVS_r(23)(nvsBaseReg);\
	ld	r24,(nvsOffset)+NVS_r(24)(nvsBaseReg);\
	ld	r25,(nvsOffset)+NVS_r(25)(nvsBaseReg);\
	ld	r26,(nvsOffset)+NVS_r(26)(nvsBaseReg);\
	ld	r27,(nvsOffset)+NVS_r(27)(nvsBaseReg);\
	ld	r28,(nvsOffset)+NVS_r(28)(nvsBaseReg);\
	ld	r29,(nvsOffset)+NVS_r(29)(nvsBaseReg);\
	ld	r30,(nvsOffset)+NVS_r(30)(nvsBaseReg);\
	ld	r31,(nvsOffset)+NVS_r(31)(nvsBaseReg);\
	lfd	f14,(nvsOffset)+NVS_f(14)(nvsBaseReg);\
	lfd	f15,(nvsOffset)+NVS_f(15)(nvsBaseReg);\
	lfd	f16,(nvsOffset)+NVS_f(16)(nvsBaseReg);\
	lfd	f17,(nvsOffset)+NVS_f(17)(nvsBaseReg);\
	lfd	f18,(nvsOffset)+NVS_f(18)(nvsBaseReg);\
	lfd	f19,(nvsOffset)+NVS_f(19)(nvsBaseReg);\
	lfd	f20,(nvsOffset)+NVS_f(20)(nvsBaseReg);\
	lfd	f21,(nvsOffset)+NVS_f(21)(nvsBaseReg);\
	lfd	f22,(nvsOffset)+NVS_f(22)(nvsBaseReg);\
	lfd	f23,(nvsOffset)+NVS_f(23)(nvsBaseReg);\
	lfd	f24,(nvsOffset)+NVS_f(24)(nvsBaseReg);\
	lfd	f25,(nvsOffset)+NVS_f(25)(nvsBaseReg);\
	lfd	f26,(nvsOffset)+NVS_f(26)(nvsBaseReg);\
	lfd	f27,(nvsOffset)+NVS_f(27)(nvsBaseReg);\
	lfd	f28,(nvsOffset)+NVS_f(28)(nvsBaseReg);\
	lfd	f29,(nvsOffset)+NVS_f(29)(nvsBaseReg);\
	lfd	f30,(nvsOffset)+NVS_f(30)(nvsBaseReg);\
	lfd	f31,(nvsOffset)+NVS_f(31)(nvsBaseReg)

#define FULLSAVE_FRAME_NVS_OFFSET(reservation) (STK_LOCAL0+((reservation)*8))

#define FULLSAVE_FRAME_ENTER(reservation, scratchReg)\
	mfcr	scratchReg;\
	std	scratchReg,STK_CR(r1);\
	mflr	scratchReg;\
	std	scratchReg,STK_LR(r1);\
	stdu	r1,-(STK_SIZE+NVS_SIZE+((reservation)*8))(r1);\
	NVS_SAVE(r1,FULLSAVE_FRAME_NVS_OFFSET(reservation))

#define FULLSAVE_FRAME_RETURN(reservation, scratchReg)\
	NVS_RESTORE(r1,FULLSAVE_FRAME_NVS_OFFSET(reservation));\
	la	r1,(STK_SIZE+NVS_SIZE+((reservation)*8))(r1);\
	ld	scratchReg,STK_LR(r1);\
	mtlr	scratchReg;\
	ld	scratchReg,STK_CR(r1);\
	mtcr	scratchReg;\
	blr

// generate 16 bytes in proper byte-order
#define HEX_BYTES_16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
	.llong 0x##a##b##c##d##e##f##g##h, 0x##i##j##k##l##m##n##o##p

// GAS and AIX disagree on the '<' vs '<<' operator
#ifndef __GNU_AS__
#define SHIFT_R(a,r) ((a)>(r))
#define SHIFT_L(a,l) ((a)<(l))
#define CHECK_SPACE(l,a) .space	(l) + SHIFT_L(1,(a)) - $
#else /* #ifndef __GNU_AS__ */
#define SHIFT_R(a,r) ((a)>>(r))
#define SHIFT_L(a,l) ((a)<<(l))
/*
 * GNU as(1) warns if ".space 0" which is a valid value for our
 * purposes here.  Furthermore, if the expression is < 0 GNU as(1)
 * accepts it as zero and emits a warning.  We want to fail in the
 * negative case.
 * NOTE: .elseif is the same as '.else .ifne'
 */
#define CHECK_SPACE(l,a) \
	.ifgt ((l) + SHIFT_L(1,(a)) - $) ;				\
	    .space (l) + SHIFT_L(1,(a)) - $ ;				\
	.elseif ((l) + SHIFT_L(1,(a)) - $) ;				\
	    .print "CHECK_SPACE: code segment too big for alignment" ;	\
	    .err ;							\
	.endif

#endif /* #ifndef __GNU_AS__ */

// WARNING:  This macro must be kept consistent with _SERROR in SysStatus.H.
#define SERROR(reg,ec,cc,gc) \
	lis	reg,0x8000|(SHIFT_R((ec),32)&0xffff); \
	ori	reg,reg,(SHIFT_R((ec),16)&0xffff); \
	sldi	reg,reg,32; \
	oris	reg,reg,(SHIFT_R((ec),0)&0xffff); \
	ori	reg,reg,SHIFT_L(((cc)&0xff),8)|((gc)&0xff)


// GAS requires a range of 0-63, 64 means 0, AIX is a little smarter
#define ROT_RIGHT(x) ((64-(x))&63)
// Need a 32-bit version as well
#define ROT_RIGHT_32(x) ((32-(x))&31)

#define TRACE_EVENT_CONST(len,layer,major,data)\
	(SHIFT_L((len),TRC_LENGTH_SHIFT)|\
	 SHIFT_L((layer),TRC_LAYER_ID_SHIFT)|\
	 SHIFT_L((major),TRC_MAJOR_ID_SHIFT)|\
	 SHIFT_L((data),TRC_DATA_SHIFT))

#define TRACE_FORM_FIRST_WORD(timeReg,len,layer,major,data)\
	rldic	timeReg,timeReg,TRC_TIMESTAMP_SHIFT,\
			TRC_TIMESTAMP_BITS-TRC_TIMESTAMP_SHIFT;\
	oris	timeReg,timeReg,\
			SHIFT_R(TRACE_EVENT_CONST(len,layer,major,data),16);\
	ori	timeReg,timeReg,\
			TRACE_EVENT_CONST(len,layer,major,data)&0xffff
