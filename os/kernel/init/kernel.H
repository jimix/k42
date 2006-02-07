#ifndef __KERNEL_H_
#define __KERNEL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernel.H,v 1.38 2004/11/05 16:24:00 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: header file for misc. function prototypes and
 * global kernel variables.
 * **************************************************************************/

#include "MemoryMgrPrimitiveKern.H"

//FIXME: what global declarations belong here, and which in module
//specific includes. e.g., a bunch of stuff is in ppccore.H that is
//similar to the extern declarations declared here.  to reduce
//inter-dependence should put all such things in module specific
//includes.
#include __MINC(kernel.H)

#ifndef __ASSEMBLER__

void initExceptionHandlers(VPNum vp);
void fixupExceptionHandlers(VPNum vp);
void initIO(VPNum vp);
void initTimer(VPNum vp);
void initKernelProcess();
void InitKernelMappings(KernelInitArgs &kernelInitArgs);
void MapTraceRegion(uval vaddr, uval paddr, uval size);
void KernelInit(KernelInitArgs& kernelInitArgs);
void KernelExit(uval killThinwire, uval physProcs, uval ctrlFlags);
sval KernelAtoi(char* p);

#endif /* #ifndef __ASSEMBLER__ */

#endif /* #ifndef __KERNEL_H_ */
