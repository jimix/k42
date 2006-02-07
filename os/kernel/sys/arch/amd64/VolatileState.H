#ifndef __VOLATILE_STATE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __VOLATILE_STATE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file
 * LICENSE.html in the top-level directory for more details.
 *
 * $Id: VolatileState.H,v 1.9 2002/04/12 13:23:41 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * Module Description:
 *****************************************************************************/

#include "misc/arch/amd64/fpr.h"   /* floating point register save area size */


struct FaultFrame {
    codeAddress rip;
    uval64      cs;		// only 16 bit selector
    uval64      rflags;		// only 32 bits eflags
    uval64      rsp;
    uval64      ss;		// only 16 bit selector
};


/* the float_save area must be aligned on a 16 byte boundary (or
 * GP fault).  Pushing faultFrame and registers leaves the stack
 * pointer 16 byte aligned for the floating point save.  */

struct VolatileState {
    // this is BIG, can we avoid XXX pdb
    uval      float_save[FPR_SAVE_AREA_SIZE/sizeof(uval)];

    uval      rax;
				// rbx not volatile
    uval      rcx;

    uval      rdx;
    uval      rbp;   // not really volatile, but needs to be saved/restored
    uval      rsi;
    uval      rdi;

    uval      r8;
    uval      r9;
    uval      r10;
    uval      r11;

    FaultFrame  faultFrame;

    codeAddress codeAddr() { return (codeAddress) faultFrame.rip; }
    codeAddress branchReg() { return (codeAddress) 0; }

} __attribute__ ((aligned (16)));

/* This structure (VolatileState) is used to define the space
   used on the stack for interrupt processing.  This needs to be
   aligned on a 16-byte boundary. */

