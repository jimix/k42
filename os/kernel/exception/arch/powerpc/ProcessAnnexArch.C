/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnexArch.C,v 1.6 2004/11/05 16:23:59 marc Exp $
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
 * Switch address spaces.  On powerpc, the kernel is mapped into every address
 * space, so switchAddressSpaceKernel() is a no-op.  For a user address space,
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
}

void
ProcessAnnex::switchAddressSpace()
{
    if (!isKernel) {
        if (exceptionLocal.currentSegmentTable != segmentTable) {
            segmentTable->switchToAddressSpace();
	    exceptionLocal.currentSegmentTable = segmentTable;
        }
    }
}

void
ProcessAnnexMachine::init(uval userMode,
			  Dispatcher *disp,
			  SegmentTable *segTable)
{
    dispatcherPhysAddr = PageAllocatorKernPinned::virtToReal(uval(disp));
    msr = PSL_EE | (userMode ? PSL_USERSET : PSL_KERNELSET);
    /* segTable is null when creating a dummy PA at startup,
     * we check here to avoid duplicating the whole init path
     */
    if(segTable != NULL) {
	asr = segTable->getASR();
    } else {
	asr = 0;
    }
}
