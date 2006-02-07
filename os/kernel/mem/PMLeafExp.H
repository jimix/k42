#ifndef __PMLEAF_EXP_H_
#define __PMLEAF_EXP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeafExp.H,v 1.1 2005/03/02 05:27:57 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has no
 * children PMs, only FCMs.
 * **************************************************************************/

#include "mem/PMLeaf.H"
#include <cobj/Factory.H>

class PMLeafExp : public PMLeaf {
protected:
    uval memConsumed;

    PMLeafExp() : memConsumed(0) {}

    DEFINE_GLOBALPADDED_NEW(PMLeafExp);

    virtual SysStatus deregisterFromFactory();
public:
    DEFINE_FACTORY_BEGIN(Factory);
       virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
       virtual SysStatus create(PMRef &pmref, PMRef parentPM);
    DEFINE_FACTORY_END(Factory);
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


    // for testing - print summary info
    virtual SysStatus print();
};
#endif /* #ifndef __PMLEAF_H_ */
