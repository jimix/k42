/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001,2002,2003,2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelInfo.C,v 1.3 2004/04/21 13:07:23 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Defines a shared structure by which the kernel publishes information.
 * **************************************************************************/

#include "kernIncs.H"
#include "KernelInfo.H"
#include <exception/HWInterrupt.H>
#include "../trace/traceInfoInit.H"

void 
KernelInfo::init(uval onHVArg,
		 uval onSimArg,
		 VPNum numaNodeArg,
		 VPNum procsInNumaNodeArg,
		 VPNum physProcArg,
		 VPNum curPhysProcsArg,
		 VPNum maxPhysProcsArg,
		 uval16 sCacheLineSizeArg,
		 uval16 pCacheLineSizeArg,
		 uval controlFlagsArg) {
    onHV                            = onHVArg;
    onSim                           = onSimArg;
    numaNode                        = numaNodeArg;
    procsInNumaNode                 = procsInNumaNodeArg;
    hwProc			    = HWInterrupt::PhysCPU(physProcArg);
    physProc                        = physProcArg;
    curPhysProcs                    = curPhysProcsArg;
    maxPhysProcs                    = maxPhysProcsArg;
    sCacheLineSize                  = sCacheLineSizeArg;
    pCacheLineSize		    = pCacheLineSizeArg;
    systemGlobal.controlFlags       = controlFlagsArg;
    //traceInfo.init(); - tracing should not be C++
    traceInfoInit(&traceInfo);    
    systemGlobal.ticksPerSecond     = 0;
    systemGlobal.epoch_sec          = 0;
    systemGlobal.epoch_usec         = 0;
    systemGlobal.mountVersionNumber = 0;
}
