/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRoot.C,v 1.20 2005/03/02 05:27:55 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/CObjRoot.H>
#include <cobj/sys/COSMgr.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>


// pool specified the kind of ref to allocate.
// in the kernel, pinned refs are allocated from the pinned part of the table
CObjRoot::CObjRoot(uval8 pool)
{
    (CORef)DREFGOBJ(TheCOSMgrRef)->alloc(this,(CORef &)myRef, pool);
}

CObjRoot::CObjRoot(RepRef ref, InstallDirective idir)
{
    myRef = (CORef) ref;
    if (idir==Install) {
	DREFGOBJ(TheCOSMgrRef)->initTransEntry((CORef)myRef, this);
    }
}

/*
 * call cleanup on a rep on this VP.
 * this is done completely asynchonously - so there
 * is no barrier to release when done
 */
/*static*/ SysStatus
CObjRoot::CleanupMsgHandler(uval repUval)
{
    CObjRep *const theRep = (CObjRep *) repUval;
    theRep->cleanup();
    return 0;
}

/*static*/ SysStatus
CObjRoot::CleanupRep(VPNum theVP, CObjRep* theRep)
{
    SysStatus retvalue;
    retvalue = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				   SysTypes::DSPID(0, theVP),
				   CleanupMsgHandler, uval(theRep));
    return(retvalue);
}

/*virtual*/ DataTransferObject *
CObjRoot::dataTransferExport(DTType dtt, VPSet transferVPSet)
{
    tassert(0, err_printf("DT-capable roots should implement this.\n"));
    return 0;
}

/*virtual*/ SysStatus
CObjRoot::dataTransferImport(DataTransferObject *dtobj,
			     DTType dtt, VPSet transferVPSet)
{
    tassert(0, err_printf("DT-capable roots should implement this.\n"));
    return 0;
}

/* virtual */ void
CObjRoot::resetTransSet()
{
    passertMsg(0, "Implement in sub class\n");
}

/* virtual */ SysStatus
CObjRoot::reAssignRef(CORef &ref)
{
    uval pool = COSMgr::pool(myRef);
    resetTransSet();
    SysStatus rc = DREFGOBJ(TheCOSMgrRef)->alloc(this,(CORef &)myRef, pool);    
    tassertMsg(_SUCCESS(rc), "alloc failed this=%p myRef=%p", this, myRef);
    ref=myRef;
    return rc;
}

/* virtual */ SysStatus
CObjRoot::deRegisterFromFactory()
{
    passertMsg(0, "Must implemenent in subclass\n");
    return -1;
}
