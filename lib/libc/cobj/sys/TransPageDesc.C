/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDesc.C,v 1.19 2005/03/02 05:27:56 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <alloc/MemoryMgrPrimitive.H>

#include "TransPageDesc.H"
#include "COSMgr.H"
#include "COSMgrObject.H"

void
TransPageDesc::init(uval page)
{
    // initialize the free list of translation entries that run through
    // this page

    GTransEntry *te = (GTransEntry *) page;
    int         numEntries = COSMgr::numGTransEntriesPerPage;
    int         i;

    teFreeList = te;
    for(i = 0; i < numEntries; i++, te++) {
	te->co = 0;
	te->next = te+1;
    }
    // backup to last entry and make it's next pointer null
    (te-1)->next = 0;

    // to help trap errors
    next = 0;
    nextForFreeEntries = 0;
    nextForHashChain = 0;
    
    pageAddr = page;
    numAllocated = 0;

    // not sure about this
    // releaseLock();
}

void
TransPageDesc::print()
{
//    acquireLock();

    printNoList();
    for(TransEntry *te = teFreeList; te != 0; te = te->next) {
	err_printf("%p ", te);
    }
    err_printf("\n");

//    releaseLock();
}

void
TransPageDesc::printNoList()
{
    err_printf("TransPageDesc: addr %lx, alloc %ld, tef %p, n %p, nff %p\n",
	    pageAddr, numAllocated, teFreeList, next, nextForFreeEntries);
}

#include <cobj/CObjRootSingleRep.H>

/*
 * This routine is intended only for debugging purposes. It attempts
 * to print the vtable pointers of all the active clustered objects on
 * this TransPageDesc.  It assumes (without any enforcement) that the
 * vtable pointers of both root objects and representatives are the
 * first words of the objects.  For a multi-rep object, the vtable
 * pointer of the root (miss-handling) object is enough to distinguish
 * the type of the object.  For a single-rep object, the root is always
 * an instance of CObjRootSingleRep, so the root's vtable pointer is
 * not interesting.  For such objects, we print the vtable pointer of
 * the object's one and only rep.
 */
void
TransPageDesc::printVTablePtrs()
{
    GTransEntry *te;
    uval numEntries = COSMgr::numGTransEntriesPerPage;
    uval i, singleRepVTable, vTable, flags;
    COSMissHandler *mh;
    CObjRootSingleRep *singleRepRoot;

    // Track down the vtable pointer of a well-known single-rep clustered
    // object.  We use it to recognize other single-rep objects.
    te = COSMgr::localToGlobal((LTransEntry *)GOBJ(TheXHandleTransRef));
    mh = te->getMH();
    singleRepVTable = *((uval *) mh);

    for (i = 0, te = (GTransEntry *) pageAddr; i < numEntries; i++, te++) {
	mh = te->getMH();
	if (mh != NULL) {
	    // A non-null miss-handler pointer indicates that the object
	    // is not free.
	    flags = te->gteData.data;
	    if (flags != 0) {
		// A non-zero flags word indicates that the object is in some
		// sort of transition.  The miss-handler pointer may or may
		// not be valid.  This state may be transient.
		err_printf("    %p: in transition (flags %lx)\n",
			   COSMgr::globalToLocal(te), flags);
	    } else {
		// Get the vtable pointer of the miss-handler.
		vTable = *((uval *) mh);
		if (vTable == singleRepVTable) {
		    // The root is of type CObjRootSingleRep, so get the
		    // vtable pointer of the rep.
		    singleRepRoot = (CObjRootSingleRep *) mh;
		    vTable = *((uval *) (singleRepRoot->getRepOnThisVP()));
		}
		err_printf("    %p: %lx\n", COSMgr::globalToLocal(te), vTable);
	    }
	}
    }
}


uval
TransPageDesc::getCOList(CODesc *coDescs, uval numDesc)
{
    GTransEntry *te;
    uval numEntries = COSMgr::numGTransEntriesPerPage;
    uval i,typeToken;
    COSMissHandler *mh;
    uval numRtn=0;
    tassertWrn(numDesc > 0, "hmm invoked without any descriptor space\n");
    for (i = 0, te = (GTransEntry *) pageAddr; i < numEntries; i++, te++) {
        if (te->isStable()) {
            mh = te->getMH();
            if (_SUCCESS(COSMgrObject::getTypeToken(mh, typeToken))) {
                if (numRtn == numDesc) break;
                coDescs[numRtn].setRef((uval)COSMgr::globalToLocal(te));
                coDescs[numRtn].setRoot((uval)mh);
                coDescs[numRtn].setTypeToken(typeToken);
                numRtn++;
            }
        } else {
            tassertWrn(0, "    %p: unstable in transition (flags = 0x%lx)\n",
                       COSMgr::globalToLocal(te), (uval)te->gteData.data);
        }
    }
//    tassertWrn(numRtn == numAllocated, "numRtn = %ld != numAllocated = %ld"
//             " numDesc = %ld\n",
//               numRtn, numAllocated, numDesc);
    return numRtn;
}
