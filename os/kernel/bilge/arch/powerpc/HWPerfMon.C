/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWPerfMon.C,v 1.7 2004/11/12 17:23:01 azimi Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "HWPerfMon.H"
#include "arch/powerpc/HWPerfMonGPUL.H"
#include "bilge/arch/powerpc/BootInfo.H"
#include "trace/traceHWPerfMon.h"
#include "exception/ExceptionLocal.H"


/* virtual */ SysStatus
HWPerfMon::HWPerfMonRoot::handleMiss(COSTransObject * &co, CORef ref,
                                     uval methodNum)
{
    passertMsg(0, "HWPerfMon should never suffer a miss");
    return -1;
}

/* virtual */ CObjRep *
HWPerfMon::HWPerfMonRoot::getRepOnThisVP()
{
    passertMsg(0, "NOT SUPPORTED");
    return 0;
}

/* virtual */ SysStatus
HWPerfMon::HWPerfMonRoot::cleanup(COSMissHandler::CleanupCmd)
{
    passertMsg(0, "NOT SUPPORTED");
    return 0;
}

/* virtual */ VPSet
HWPerfMon::HWPerfMonRoot::getTransSet()
{
    VPSet dummy;
    passertMsg(0, "NOT SUPPORTED");
    return dummy;
}

/* virtual */ VPSet
HWPerfMon::HWPerfMonRoot::getVPCleanupSet()
{
    VPSet dummy;
    passertMsg(0, "NOT SUPPORTED");
    return dummy;
}

/* static */ SysStatus
HWPerfMon::VPInit()
{
    switch (_BootInfo->cpu_version) {
    case VER_970FX:
    case VER_BE_PU:
        return HWPerfMonGPUL::VPInit();
    }
    return 0;
}

/* static */ SysStatus
HWPerfMon::ClassInit(VPNum vp)
{
    static HWPerfMonRoot *root;
    HWPerfMon     *rep;
    CORef          ref;
    LTransEntry *lte;

    if (vp == 0) {
        root = new HWPerfMonRoot();
    }

    ref = (CORef)root->getRef();

    switch (_BootInfo->cpu_version) {
    case VER_970:
    case VER_970FX:
        rep = new HWPerfMonGPUL(root
#ifdef MAMBO_SUPPORT
	, GPUL_HARDWARE
#endif
	);
        break;
    case VER_BE_PU:
        rep = new HWPerfMonGPUL(root
#ifdef MAMBO_SUPPORT 
	, GPUL_MAMBO);
#endif 
        break;

    default:
        rep = new HWPerfMon(root);
    }

    // Install the new representative in the Translation table of this vp.
    lte=(LTransEntry *)ref;
    lte->setCO(rep);
    // Install it in exceptionLocal structure for this vp as well.
    // To ensure that interrupts can be correctly redirected to the correct
    // rep.
    exceptionLocal.setHWPerfMonRep(rep);

    return 0;
}


