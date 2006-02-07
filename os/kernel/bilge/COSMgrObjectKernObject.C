/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMgrObjectKernObject.C,v 1.7 2005/01/26 03:21:51 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The Clustered Object System Manager (which itself is a
 *    Clustered Object.  There is one rep per vp.  which manges the local trans
 *    cache and the vp's portion of the main trans table.
 * **************************************************************************/

#include <kernIncs.H>
#include <cobj/sys/TransPageDescList.H>
#include <cobj/sys/COVTable.H>
#include <scheduler/Scheduler.H>
#include <trace/traceClustObj.h>
#include <misc/hardware.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <misc/ListSimple.H>
#include <misc/ListArraySimple.H>
#include <cobj/CObjRootMultiRep.H>
#include <mem/FRLTransTable.H>
#include <mem/RegionPerProcessor.H>
#include <mem/FCMPrimitiveKernel.H>
#include "COSMgrObjectKernObject.H"

/* COSMgr requires special care in initialization as no other     *
 * well know Clustered Objects including the allocators are       *
 * available.  Both the root and the reps for each processor are  *
 * created by hand and installed explicitly.                      */
SysStatus
COSMgrObjectKernObject::ClassInit(VPNum vp, MemoryMgrPrimitive *pa)
{
    static COSMgrObjectKernObject::COSMgrObjectKernObjectRoot *root;
    COSMgrObjectKernObject *rep;

    passertMsg(COSMAXVPS == Scheduler::VPLimit, "FIXME!!!!! Do this right!!\n");
    // Create the root on the boot processor
    if (vp == 0) root = new(pa) COSMgrObjectKernObjectRoot();
    err_printf("COSMgrObjectKernObject::ClassInit root=%p vp=%ld\n",root,vp);

    // Create a rep on this processor
    rep = new(pa) COSMgrObjectKernObject();
    rep->setRoot(root);

    // Create/Map Translation tables
    vpMapCOTransTables(vp,&(root->theDefaultObject),pa);

    // do per rep vp initialization.  Avoid going through the LTransTable
    rep->vpInit(vp, CObjGlobalsKern::numReservedEntriesKern, pa);

    // On boot processor set the root into the Global TransTable
    if (vp == 0) rep->initTransEntry((CORef)GOBJ(TheCOSMgrRef), root);

    // Explicitly add the new rep to the root as the rep for this vp
    root->init(vp,rep);

    // Note it is now safe to access the COSMgr via the trans table on this
    // vp.  A normal miss will occur.

    return 1;
}

/* virtual */ SysStatus
COSMgrObjectKernObject::vpMaplTransTablePaged(VPNum vp, uval useExistingAddr)
{
    FCMRef fcmRef;
    FRRef frRef;
    RegionRef regionRef;
    uval regionVaddr;

    // If things fail here we are in big trouble so no pointing return codes
    // :-)
    if (vp == 0) {
        // setup memory for pagable portion of the global trans table
        FCMPrimitiveKernel::Create(fcmRef);
        FRPlaceHolderPinned::Create(frRef);
        DREF(frRef)->installFCM(fcmRef);
	RegionDefaultKernel::CreateFixedLen(
	    regionRef, (ProcessRef)GOBJ(TheProcessRef), regionVaddr,
	    gTransTablePagableSize, 0, frRef, 0, 0,
	    AccessMode::noUserWriteSup);
        gTransTablePaged = (GTransEntry*)regionVaddr;

        // setup memory for pagable portion of the local trans table
        FRLTransTablePinned::Create(frRef,
                                    *(uval*)&(COGLOBAL(theDefaultObject)));
	RegionPerProcessorKernel::CreateFixedLen(
	    regionRef, (ProcessRef)GOBJ(TheProcessRef),
	    regionVaddr, (uval)gTransTablePagableSize, 0, frRef, 0, 0,
	    AccessMode::noUserWriteSup);
        lTransTablePaged = (LTransEntry *)regionVaddr;
    }

    // Setup managment of this vps portion of the global table
    pagablePageDescList.init((uval)gTransTablePaged +
                             (uval)(vp * gPartPagableSize), gPartPagableSize);

    return 0;
}

/* static */ SysStatusUval 
getKnownTypeList(COTypeDesc *typeDesc, uval numDescs) {
    return 0;
}
