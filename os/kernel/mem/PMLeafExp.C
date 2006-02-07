/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeafExp.C,v 1.1 2005/03/02 05:27:57 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has no
 * children PMs, only FCMs.
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include "mem/PMLeafExp.H"

DECLARE_FACTORY_STATICS(PMLeafExp::Factory);

class PMLeafExpRoot : public CObjRootSingleRep {
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }

    virtual DataTransferObject * dataTransferExport(DTType dtt, VPSet dtVPSet) {
        passertMsg(dtt == DTT_TEST, "unknown DDT\n");
        return (DataTransferObject *)this;
    }

    virtual SysStatus dataTransferImport(DataTransferObject *data, DTType dtt,
                                        VPSet dtVPSet) {
        err_printf("PMLeafExp::dataTransferImport: data=%p\n", data);
        PMLeafRoot *oldroot = (PMLeafRoot *)data;
        err_printf("oldroot=%p with rep=%p\n",oldroot, oldroot->therep);
        passertMsg(oldroot->exported.isEmpty(), "old root's exported list"
                   " is not empty\n");
        *((PMLeaf *)this->therep) = *((PMLeaf *)(oldroot->therep));
        this->therep->setRoot(this);
//        Scheduler::DelaySecs(60);
        return 0;
    }
public:
    DEFINE_GLOBAL_NEW(PMLeafExpRoot);

    PMLeafExpRoot(PMLeafExp *rep) : CObjRootSingleRep((CObjRep *)rep) {
      /* empty body */
    }
    PMLeafExpRoot(PMLeafExp *rep, RepRef ref,
		   CObjRoot::InstallDirective idir=CObjRoot::Install) :
	CObjRootSingleRep((CObjRep *)rep, ref, idir) {
	/* empty body */
    }

    virtual SysStatus deRegisterFromFactory()
        {
            return DREF_FACTORY_DEFAULT(PMLeafExp)->
                deregisterInstance((CORef)getRef());
        }
};

/* virtual */ SysStatus
PMLeafExp::Factory::create(PMRef &pmref, PMRef parentPM)
{
    //err_printf("Creating PMLeaf\n");
    PMLeafExp *pm;
    pm = new PMLeafExp();
    tassert(pm!=NULL, err_printf("No mem for PMLeaf\n"));
    PMLeafExpRoot *root = new PMLeafExpRoot(pm);
    pmref = (PMRef) root->getRef();
    //err_printf("PMLeaf %lx created with parent %lx\n", pmref, parentPM);
    pm->parentPM = parentPM;
    DREF(parentPM)->attachPM(pmref);
    registerInstance((CORef)pmref);
    return 0;
}

/* virtual */ SysStatus
PMLeafExp::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    PMLeafExp *rep = new PMLeafExp;
    root = new PMLeafExpRoot(rep, (RepRef)ref, CObjRoot::skipInstall);
    tassertMsg(ref == (CORef)root->getRef(), "Opps ref=%p != root->getRef=%p\n",
               ref, root->getRef());
    registerInstance((CORef)root->getRef());
    return 0;
}


/* virtual */ SysStatus
PMLeafExp::allocPages(FCMRef fcm, uval &ptr, uval size, 
                      uval pageable, uval flags, VPNum node)
{
    SysStatus rc = PMLeaf::allocPages(fcm, ptr, size, pageable, flags, node);
    if (_SUCCESS(rc)) {
        lock.acquire();
        memConsumed += size;
        lock.release();
    }
    return rc;
}

/* virtual */ SysStatus
PMLeafExp::allocListOfPages(FCMRef fcm, uval count, FreeFrameList *ffl)
{
    SysStatus rc = PMLeaf::allocListOfPages(fcm, count, ffl);
    if (_SUCCESS(rc)) {
        // ffl is hard coded to be filled with PAGE_SIZED units
        lock.acquire();
        memConsumed += ffl->getCount() * PAGE_SIZE;
        lock.release();
    }
    return rc;
}

/* virtual */ SysStatus
PMLeafExp::deallocPages(FCMRef fcm, uval paddr, uval size)
{
    SysStatus rc = PMLeaf::deallocPages(fcm, paddr, size);
    if (_SUCCESS(rc)) {
        tassertWrn(memConsumed >= size, "Yikes deallocated size=%ld when "
                   "memConsumed=%ld\n", size, memConsumed);
        lock.acquire();
        memConsumed -= size;
        lock.release();
    }
    return rc;
}

/* virtual */ SysStatus
PMLeafExp::deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)
{
    uval count = ffl->getCount();
    SysStatus rc = PMLeaf::deallocListOfPages(fcm, ffl);
    if (_SUCCESS(rc)) {
        tassertWrn(memConsumed >= count * PAGE_SIZE, "Yikes deallocated"
                   " count*PAGE_SIZE = %ld when memConsumed=%ld\n",
                   count * PAGE_SIZE, memConsumed);
        lock.acquire();
        memConsumed -= count * PAGE_SIZE;
        lock.release();
    }
    return rc;
}

/* virtual */SysStatus 
PMLeafExp::deregisterFromFactory()
{
    return DREF_FACTORY_DEFAULT(PMLeafExp)->
        deregisterInstance((CORef)getRef());
}

/* virtual */ SysStatus
PMLeafExp::print()
{
    PMLeaf::print();
    lock.acquire();
    err_printf("PMLeafExp::memConsumed = %ld bytes\n",memConsumed);
    lock.release();
    return 0;
}
