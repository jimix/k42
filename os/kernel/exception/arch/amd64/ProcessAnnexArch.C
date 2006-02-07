/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnexArch.C,v 1.9 2002/05/02 20:15:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the machine-dependent parts of the
 *                     exception-level kernel structure that represents a
 *                     virtual processor.
 * **************************************************************************/

#include <kernIncs.H>
#include "exception/ExceptionLocal.H"
#include "exception/ProcessAnnex.H"
#include "mem/SegmentTable.H"
#include "mem/PageAllocatorKernPinned.H"

/*
 * Switch address spaces.  On amd64, the kernel is mapped into every address
 * space, so switchAddressSpaceKernel() is a no-op.
 *
 * For a user address space,
 * it's worth checking whether we're already in the target space because this
 * case happens every time we duck into the kernel and back out quickly.
 */

void
ProcessAnnex::switchAddressSpaceUser()
{
    if (exceptionLocal.currentSegmentTable != segmentTable) {
	segmentTable->switchToAddressSpace();
	exceptionLocal.currentSegmentTable = segmentTable;
    }
}

void
ProcessAnnex::switchAddressSpaceKernel()
{
  //    segmentTable->switchToAddressSpace();
  //    exceptionLocal.currentSegmentTable = segmentTable;
}

void
ProcessAnnex::switchAddressSpace()
{
  /* FIXME -- X86-64 why should being a kernel process make a difference???
     XXX heureusement unused !!!!!!!!!!!!!!!!*/
  if (!isKernel) {
    if (exceptionLocal.currentSegmentTable != segmentTable) {
      segmentTable->switchToAddressSpace();
      exceptionLocal.currentSegmentTable = segmentTable;
    }
  }
}

#ifdef notdefpdb
void
ProcessAnnexMachine::init(uval userMode,
			  Dispatcher *disp,
			  SegmentTable *segTable)
{
    /* FIXME -- X86-64 */
    dispatcherPhysAddr = PageAllocatorKernPinned::virtToReal(uval(disp));
    msr = PSL_EE | (userMode ? PSL_USERSET : PSL_KERNELSET);
}
#endif /* #ifdef notdefpdb */

void
ProcessAnnexMachine::switchExcStateSaveArea() 
{
  /* On the AMD64 architecture, the hardware will save the first
   * few parts of the volatile state -- the stack pointer and
   * instruction address.  The rest is saved by
   * PART_VOLATILE_FRAME_SAVE().  The location of the area to
   * save these first few registers is defined by:
   
   * (a) The interrupt type picks an entry from the IDT
   * (b) The IDT entry specifies the TSS
   * (c) The ist field of the IDT says which part of the TSS to use
   * (d) The TSS lists RSP0, RSP1, RSP2 and IST1, IST2, .... IST7.
   *     which define stack areas.
   
   * If the ist field is zero, we use the normal RSP0 field; if
   * the ist field is non-zero, we use the specified IST* field.
   
   * Our design says that we always use IST1.  We change IST1 to
   * point to one of the 3 save areas in the "struct Dispatcher"
   * as appropriate.  To implement this, we set (and reset) the
   * IST1 field in the TSS structure to point to the specified
   * save area.
   
   * One exception is that the page fault interrupt has an ist
   * field of 0; so it uses RSP0 (unless it is a page fault in
   * kernel mode when it just uses the existing stack pointer).
   * So we set both RSP0 and IST1 to point to the save area.
   
   * Once the registers have been saved in the save area, we will
   * need to always switch to an appropriate kernel stack for
   * parameters, return addresses, local variables, and so on.
   * This is provided by the EXCEPTION_STACK() macro.  */

  /* the address of the save area must be a multiple of 16 */
  tassertWrn(((esp0 & 0xF) == 0), "Stack not a multiple of 16: 0x%lx\n", esp0);
  
  // set task exception stack (RSP0) to current exc-level save area.
  exceptionLocal.x86Task.RSP0 = esp0;
  
  // set task exception stack (IST1) to current exc-level save area.
  exceptionLocal.x86Task.IST1 = esp0;
}
