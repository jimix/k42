/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernel.H,v 1.13 2004/11/12 21:25:47 marc Exp $
 *****************************************************************************/
#ifndef __KERNEL_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __KERNEL_H_ */

#ifndef __ASSEMBLER__

uval ioReserve(uval len);
uval ioMap(uval vaddr, uval paddr, uval len);
uval ioReserveAndMap(uval paddr, uval len);

struct ExceptionLocal;

class InvertedPageTable;
struct RfiState;
struct KernelInitArgs {
    MemoryMgrPrimitiveKern memory;
    volatile uval *barrierP;
    VPNum vp;
    InvertedPageTable *sharedIPT;	// page table object to share
    uval pageTable;			// or new page table origin
    // secondary start values
    uval64 iar;				// secondary start location
    uval64 toc;
    uval64 msr;				
    uval64 sdr1;
    uval stackAddr;
    ExceptionLocal *elocal;		// vmapsr elocal for new cpu
    uval relocal;			// real address of new elocal
};

// Global variable to be used ONLY in code that cannot use KernelInfo::OnSim().
extern uval _OnSim;
extern uval _OnHV;

#endif /* #ifndef __ASSEMBLER__ */
