#ifndef __POWERPC_MPINIT_H_
#define __POWERPC_MPINIT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPinit.H,v 1.7 2004/11/12 21:25:46 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *
 *          struct definition for communication between C++ and ASM
 *          code for MP initialization
 *
 * **************************************************************************/

#include "mem/SegmentTable.H"

#if 0
struct XferArea {
    SegmentTable *segTable;		// hw segment table
    uval    numbCPUToStart;		// number of cpu being started
    uval    secondaryVirtBase;		// virtual addr of real 0
    uval    allocStart;			// area beyond stack
    uval    secondaryBootMem;		// virt addr of memory for the processor
    uval    rSecondaryBootMem;		// real addr of same
    uval    segLoadType;		// see InvertedPageTable.H
    uval64  iar;			// initial IAR value
    uval64  msr;			// initial MSR value
    uval    stackAddr;			// initial stack pointer value
    uval    toc;			// initial TOC value
    InvertedPageTable *sharedIPT;	// page table to share or null
    volatile uval* barrierP;		// addr of barrier (count of to-be-started processors)
    RfiState* boltedRfiStatePage;       // addr of bolted storage
    uval    rcpu_starter;		// for "pingMsg" test code in MPinit.C
};
#endif

#endif /* #ifndef __POWERPC_MPINIT_H_ */
