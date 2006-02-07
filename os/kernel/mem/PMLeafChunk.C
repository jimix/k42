/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeafChunk.C,v 1.7 2005/06/27 06:15:52 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has no
 * children PMs, only FCMs.
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PMLeafChunk.H"
#include <stub/StubKBootParms.H>
#include "trace/traceMem.h"
#include <stdlib.h>

DECLARE_FACTORY_STATICS(PMLeafChunk::Factory);

uval
PMLeafChunk::chunkSize = 512*1024;

class PMLeafChunkRoot : public CObjRootSingleRep {
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }

    virtual DataTransferObject *dataTransferExport(DTType dtt, VPSet dtVPSet) {
        passertMsg(dtt == DTT_TEST, "unknown DDT\n");
        return (DataTransferObject *)this;
    }

    virtual SysStatus dataTransferImport(DataTransferObject *data, DTType dtt,
                                        VPSet dtVPSet) {
	passertMsg(0, "NYI");
	return -1;
    }
public:
    DEFINE_GLOBAL_NEW(PMLeafChunkRoot);

    PMLeafChunkRoot(PMLeafChunk *rep)
	: CObjRootSingleRep((CObjRep *)rep) 
    {
	/* empty body */
    }

    PMLeafChunkRoot(PMLeafChunk *rep, RepRef ref,
		   CObjRoot::InstallDirective idir=CObjRoot::Install)
	: CObjRootSingleRep((CObjRep *)rep, ref, idir) 
    {
	/* empty body */
    }

    virtual SysStatus deRegisterFromFactory() {
	return DREF_FACTORY_DEFAULT(PMLeafChunk)->
	    deregisterInstance((CORef)getRef());
    }
};

/* virtual */ SysStatus
PMLeafChunk::Factory::create(PMRef &pmref, PMRef parentPM)
{
    //err_printf("Creating PMLeaf\n");
    PMLeafChunk *pm;
    pm = new PMLeafChunk();
    tassert(pm!=NULL, err_printf("No mem for PMLeaf\n"));
    PMLeafChunkRoot *root = new PMLeafChunkRoot(pm);
    pmref = (PMRef) root->getRef();
    //err_printf("PMLeaf %lx created with parent %lx\n", pmref, parentPM);
    pm->parentPM = parentPM;
    DREF(parentPM)->attachPM(pmref);
    registerInstance((CORef)pmref);
    return 0;
}

/* virtual */ SysStatus
PMLeafChunk::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    PMLeafChunk *rep = new PMLeafChunk;
    root = new PMLeafChunkRoot(rep, (RepRef)ref, CObjRoot::skipInstall);
    tassertMsg(ref == (CORef)root->getRef(), "Opps ref=%p != root->getRef=%p\n",
               ref, root->getRef());
    registerInstance((CORef)root->getRef());
    return 0;
}


uval PMLeafChunk::freedChunkCount = 0;
uval PMLeafChunk::freedSingleCount = 0;
BLock PMLeafChunk::freedLock;

PMLeafChunk::~PMLeafChunk()
{
    TraceOSMemProcessMemoryUsage(maxMemConsumed);
    uval ptr, size;
    SysStatus rc;
    uval memFreed = 0;

//     err_printf("__MaxMemConsumed: %li\n", maxMemConsumed);
    lock.acquire();
    while ( freeFrameList.getChunkOfFrames(&ptr, &size) ) {
	if (size==PAGE_SIZE) {
	    freedLock.acquire();
	    freedSingleCount++;
	    freedLock.release();
	} else {
	    freedLock.acquire();
	    freedChunkCount++;
	    freedLock.release();
	}
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    deallocPages(ptr, size);
	memFreed += size;
	passertMsg((_SUCCESS(rc)), "dealloc failed (PMLeafChunk)\n");
    }
    
//     err_printf("__MemFreed: %li\n", memFreed);
    lock.release();
}

void
PMLeafChunk::ClassInit()
{
    SysStatus rc;
    char leafChunkSizeStr[20];

    rc = StubKBootParms::_GetParameterValue("K42_PMLEAF_CHUNK_SIZE",
					leafChunkSizeStr, 20);

    if (_SUCCESS(rc)) {
	chunkSize = baseAtoi(leafChunkSizeStr);
	passertMsg( (chunkSize!=0) && (chunkSize % 4096 == 0),
		    "Invalid K42_PMLEAF_CHUNK_SIZE: %lu\n", chunkSize);
	err_printf("PMLEAF_CHUNK_SIZE set to %lu\n", chunkSize);
    }
}

/* virtual */ SysStatus
PMLeafChunk::allocPages(FCMRef fcm, uval &ptr, uval size, 
                      uval pageable, uval flags, VPNum node)
{
    SysStatus rc = 0;

    if (size!=PAGE_SIZE) {
	// Large page requests go through to PMLeaf as normal
	rc = PMLeaf::allocPages(fcm, ptr, size, pageable, flags, node);
	return rc;
    }

    lock.acquire();
    if (freeFrameList.getCount()==0) {
	// Need to grab another chunk of frames
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(ptr, chunkSize, flags, node);
	passertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	memAllocated += chunkSize;
	
	// Put large chunk into free frame list
	freeFrameList.freeChunkOfFrames(ptr, chunkSize);
    }

    ptr = freeFrameList.getFrame();
    
    passertMsg(ptr!=0, "Could not allocate memory?");

    if (_SUCCESS(rc)) {
        memConsumed += size;
	if (memConsumed > maxMemConsumed) maxMemConsumed = memConsumed;
    }

    lock.release();
    return rc;
}

/* virtual */ SysStatus
PMLeafChunk::allocListOfPages(FCMRef fcm, uval count, FreeFrameList *ffl)
{
    SysStatus rc = 0;

    lock.acquire();
    uval ptr;
    for (uval i=0; i<count; i++) {
	ptr = freeFrameList.getFrame();
	if (ptr==0) {
	    // Need to grab another chunk of frames
	    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
		allocPages(ptr, chunkSize);
	    passertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	    memAllocated += chunkSize;
	
	    // Put large chunk into free frame list
	    freeFrameList.freeChunkOfFrames(ptr, chunkSize);

	    // Use one of those pages
	    ptr = freeFrameList.getFrame();
	}
	ffl->freeFrame(ptr);
    }

    if (_SUCCESS(rc)) {
        // ffl is hard coded to be filled with PAGE_SIZED units
        memConsumed += ffl->getCount() * PAGE_SIZE;
	if (memConsumed > maxMemConsumed) maxMemConsumed = memConsumed;
    }

    lock.release();
    return rc;
}

/* virtual */ SysStatus
PMLeafChunk::deallocPages(FCMRef fcm, uval paddr, uval size)
{
    SysStatus rc = 0;
    if (size==PAGE_SIZE) {
	lock.acquire();
	freeFrameList.freeFrame(paddr);
	lock.release();
    } else {
	// Large page requests go through PMLeaf
	rc = PMLeaf::deallocPages(fcm, paddr, size);
    }
    
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
PMLeafChunk::deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)
{
    uval count = ffl->getCount();
    tassertWrn(memConsumed >= count * PAGE_SIZE, "Yikes deallocated"
	       " count*PAGE_SIZE = %ld when memConsumed=%ld\n",
	       count * PAGE_SIZE, memConsumed);
    lock.acquire();
    freeFrameList.freeList(ffl);
    memConsumed -= count * PAGE_SIZE;
    lock.release();
    return 0;
}

/* virtual */SysStatus 
PMLeafChunk::deregisterFromFactory()
{
    return DREF_FACTORY_DEFAULT(PMLeafChunk)->
        deregisterInstance((CORef)getRef());
}

/* virtual */ SysStatus
PMLeafChunk::print()
{
    PMLeaf::print();
#if 0
    lock.acquire();
    err_printf("PMLeafChunk::memConsumed = %ld bytes, "
	       "maxMemConsumed = %ld bytes\n", memConsumed, maxMemConsumed);
    lock.release();
#endif
    return 0;
}

SysStatus 
PMLeafChunk::freeCachedFrames() 
{
    uval ptr, size;
    SysStatus rc;

    lock.acquire();

    while ( freeFrameList.getChunkOfFrames(&ptr, &size) ) {
	if (size==PAGE_SIZE) {
	    freedLock.acquire();
	    freedSingleCount++;
	    freedLock.release();
	} else {
	    freedLock.acquire();
	    freedChunkCount++;
	    freedLock.release();
	}
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    deallocPages(ptr, size);
    }

    lock.release();
    return 0;
}
