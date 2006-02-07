/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWPerfMon.C,v 1.3 2002/03/26 15:10:16 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/
#include "kernIncs.H"
#include "bilge/HWPerfMon.H"
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

    rep=new HWPerfMon(root);
    
    // Install the new representative in the Translation table of this vp.
    lte=(LTransEntry *)ref;
    lte->setCO(rep);
    // Install it in exceptionLocal structure for this vp as well.
    // To ensure that interrupts can be correctly redirected to the correct
    // rep.
    exceptionLocal.setHWPerfMonRep(rep);

    return 0;
}

