#ifndef __PMLEAF_CHUNK_H_
#define __PMLEAF_CHUNK_H__
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeafChunk.H,v 1.4 2005/06/27 06:15:53 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: LargePage support for Leaf PMs 
 * (generally for a given Process); has no children PMs, only FCMs.
 * **************************************************************************/

#include "mem/PMLeaf.H"
#include "FreeFrameListChunk.H"
#include <cobj/Factory.H>

class PMLeafChunk : public PMLeaf {
public:
    static uval chunkSize;
    static uval freedChunkCount;
    static uval freedSingleCount;

private:
    uval memConsumed;
    uval maxMemConsumed;
    uval memAllocated;

    FreeFrameListChunk freeFrameList;
    static BLock freedLock;

protected:

    PMLeafChunk() : memConsumed(0), maxMemConsumed(0), memAllocated(0),
		    freeFrameList(chunkSize)
    {
	passertMsg(chunkSize % PAGE_SIZE == 0, "Invalid chunk size used");
    };

    DEFINE_GLOBALPADDED_NEW(PMLeafChunk);

public:

    ~PMLeafChunk();

    DEFINE_FACTORY_BEGIN(Factory);
       virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
       virtual SysStatus create(PMRef &pmref, PMRef parentPM);
    DEFINE_FACTORY_END(Factory);

    static void ClassInit();

    static SysStatus Create(PMRef &pmref, PMRef parentPM);
   
    // allocates contiguous region of memory,
    virtual SysStatus allocPages(FCMRef fcm, uval &ptr, uval size,
				 uval pageable, 
				 uval flags=0,
				 VPNum node=PageAllocator::LOCAL_NUMANODE);
    // list of standard size, standard node, frames
    virtual SysStatus allocListOfPages(FCMRef fcm, uval count, 
				       FreeFrameList *ffl);
    // free memory at specified physical address
    virtual SysStatus deallocPages(FCMRef fcm, uval paddr, uval size);

    // free memory at addresses specified in a list
    virtual SysStatus deallocListOfPages(FCMRef fcm, FreeFrameList *ffl);

    virtual SysStatus deregisterFromFactory();

    virtual SysStatus print();

    virtual SysStatus freeCachedFrames();
};

#endif /* #ifndef __PMLEAF_H_ */
