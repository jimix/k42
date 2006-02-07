/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memoryMap.C,v 1.25 2004/03/09 19:32:29 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Check validity of well-known symbols.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <sys/memoryMap.H>
#include <sys/Dispatcher.H>
#include <sys/ppccore.H>
#include <scheduler/Scheduler.H>
#include <trace/trace.H>

/*
 * These global pointers are declared simply for debugging convenience.  No
 * actual C++-level definitions for the processor-specific structures exist,
 * so the debugger has no type information for the structures themselves.
 */
const AllocPool			*allocLocalDbg		= &allocLocal[0];
const ActiveThrdCnt		*activeThrdCntLocalDbg	= &activeThrdCntLocal;
const LTransEntry		*lTransTableLocalDbg	= &lTransTableLocal[0];
const volatile KernelInfo	*kernelInfoLocalDbg	= &kernelInfoLocal;
const ExtRegs			*extRegsLocalDbg	= &extRegsLocal;

void
memoryMapCheck(void)
{
    /*
     * The following wassert's are here simply to keep garbage-collecting
     * linkers from eliminating the *LocalDbg variables.
     */
    tassertWrn(allocLocalDbg         == &allocLocal[0],       "Huh?");
    tassertWrn(activeThrdCntLocalDbg == &activeThrdCntLocal,  "Huh?");
    tassertWrn(lTransTableLocalDbg   == &lTransTableLocal[0], "Huh?");
    tassertWrn(kernelInfoLocalDbg    == &kernelInfoLocal,     "Huh?");
    tassertWrn(extRegsLocalDbg       == &extRegsLocal,        "Huh?");
}
