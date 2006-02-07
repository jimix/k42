#ifndef __TRANS_ENTRY_H_
#define __TRANS_ENTRY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransEntry.H,v 1.16 2005/01/26 03:21:50 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The definition of an entry in the object
 * translation table
 * **************************************************************************/

#ifdef IN_C
#define COSTransObject  uval;
#define COSMissHandler  uval;
#define COSTransObject  uval;
#else /* #ifdef IN_C */
#include "COSTransObject.H"
#include "COSMissHandler.H"
#include "COSTransObject.H"
#include "COSDefaultObject.H"
#endif /* #ifdef IN_C */
#include <sys/KernelInfo.H>

// FIXME: change to TransEntry when do swap to new clustered object model
/*
 * This really should be an embedded struct of the COSMgr
 * The need for knowledge of it's size in C/asm files makes
 * this difficult.
 */
struct TransEntry {

    struct GTransData : BitStructure {
	__BIT_FIELD(32, mhcount, BIT_FIELD_START);
	__BIT_FIELD(1, writing, mhcount);
	__BIT_FIELD(1, switching, writing);
	__BIT_FIELD(1, pendingDestroy, switching);
	__BIT_FIELD(1, destroying, pendingDestroy);
	__BIT_FIELD(1, cleaned, destroying);
	__BIT_FIELD(27, filler, cleaned);
    };

    union {
	COSTransObject *co;
	COSMissHandler *mh;
    };
    union {
	COSTransObject *tobj;
	TransEntry    *next;
	//uval            data;
	GTransData gteData;
    };
};


#ifndef IN_C
/*
 * Warning:
 * Do not add any data members to these.  The size
 * of these structs must be the same as TransEntry
 */
struct GTransEntry : TransEntry {
    void setMH(COSMissHandler *m) { mh = m; }
    COSMissHandler *getMH()       { return mh; }

    void resetBits() {
	gteData.data = uval64(0);
    }

    uval isStable() {
        // A non-zero flag indicates that the object is in some
        // sort of transition.  The miss-handler pointer may or may
        // not be valid.  This state may be transient.
        return !(gteData.writing() || gteData.switching()|| 
                 gteData.pendingDestroy() || gteData.destroying() ||
                 gteData.cleaned());
    }

    uval mhReadEntry() {
	GTransData olocalData, localData;
	uval success = 0;

	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS:
	    // if entry is on free list someone has used a bogus ref
	    if( !co ) {
		// during testing, we should not deref a free ref.
		// during running, should we panic?  No.
		tassertWrn(0, "miss on free gte\n");
		return 0;
	    }
	    // if object is being destroyed we are not allowed to enter
	    if (localData.destroying() == 1) {
		tassertWrn(0, "miss on object being destroyed\n");
		return 0;
	    }

	    // if the write bit is set we must retry
	    // FIXME:  we are giving writers priority potential for reader
	    //         starvation
	    if (localData.writing() == 1) {
		if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
		    err_printf("spin1(%ld)...", uval(localData.writing()));
		}
		continue;
	    }

	    // OK PRECONDITONS MET
	    localData.mhcount(localData.mhcount()+1);  // inc count
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success);  // fails if something has changed must rety
	return 1;
    }

    void mhReadExit() {
	GTransData olocalData, localData;
	uval success = 0;

	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS:
	    // Just some sanity checks
	    tassert(localData.mhcount() > 0, err_printf("oops: mhcount=0\n"));
	    tassert(localData.writing() == 0, err_printf("oops: writing=1\n"));

	    // OK PRECONDITONS MET
	    localData.mhcount(localData.mhcount()-1); // dec count
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success); // fails if something has changed must rety
    }

    uval mhSwitchEntry() {
	GTransData olocalData, localData;
	uval success = 0;

	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS:
            // turn away attempts to switch when the object is being destroyed
	    if (localData.destroying() != 0) return 0;

	    if (localData.pendingDestroy() != 0) return 0;

	    // turn away attempts to switch when the object is being switched
	    if (localData.switching() != 0) return 0;

	    // wait until there are no more readers in progress
	    if (localData.mhcount() > 0) {
		if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
		    err_printf("spin2(%ld)...", localData.mhcount());
		}
		// retry
		continue;
	    }

	    // OK: PRECONDITONS MET
	    localData.switching(1);  // set lock bit
	    localData.writing(1);    // set writing bit

	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success);

	return 1;
    }


    // No explicit exit method as this is the last action on an TransEntry
    enum DestroyStatus { DESTROYINPROGRESS, DESTROYPENDING, DESTROYSTARTED };

    DestroyStatus mhDestroyEntry() {
	GTransData olocalData, localData;
	uval success = 0;
	DestroyStatus rtn = DESTROYSTARTED; // avoid warning
	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS
	    // Someone has already called destroy
	    if (localData.destroying() != 0) {
		return DESTROYINPROGRESS;
	    }

	    // switch is in progress can not destroy yet
	    // FIXME: Not sure what the right thing to do is
	    if (localData.switching() != 0) {
		// Mark the fact that destroy has been called
		// so that the switch exit code can ensure the
		// detroy is reinitiated.

		// OK PRECONDITONS MET
		localData.pendingDestroy(1);
		rtn = DESTROYPENDING;
	    } else {
		// wait for current readers to complete
		if (localData.mhcount() > 0) {
		    err_printf("spin3(%ld)...", localData.mhcount());
		    continue;
		}

		// OK PRECONDITONS MET
		localData.destroying(1);
		rtn = DESTROYSTARTED;
	    }
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success); // fails if something has changed must rety

	return rtn;

    }

    uval mhIsMarkedCleaned() {
	return gteData.cleaned();
    }

    void mhMarkCleaned() {
	GTransData olocalData, localData;
	uval success = 0;

	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS
	    // Just some sanity checks
	    tassert(localData.mhcount() == 0, err_printf("oops: mhcount>0\n"));
	    tassert(localData.writing() == 0, err_printf("oops: writing=1\n"));
	    tassert(localData.switching() == 0,
		    err_printf("oops: switching=1\n"));
	    tassert(localData.destroying() == 1,
		    err_printf("oops: destroying=0\n"));


	    // OK PRECONDITONS MET
	    localData.cleaned(1);
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success); // fails if something has changed must rety

    }

    void mhWriteComplete() {
	GTransData olocalData, localData;
	uval success = 0;

	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS
	    // just some sanity checks
	    tassert(localData.writing() == 1,
		    err_printf("Didn't grab write bit\n"));
	    tassert(localData.destroying() == 0,
		    err_printf("oops: destroying bit set\n"));
	    // OK PRECONDTIONS MET
	    localData.writing(0); // reset writing bit
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success); // fails if something has changed must rety
    }

    enum SwitchCleanupStatus { DESTROYNOTREQUIRED, DESTROYREQUIRED};

    SwitchCleanupStatus mhSwitchExit() {
	GTransData olocalData, localData;
	uval success = 0;
	SwitchCleanupStatus rtn=DESTROYNOTREQUIRED;
	do {
	    olocalData.data = gteData.data;
	    localData.data  = olocalData.data;

	    // PRECONDITONS:
	    // just some sanity checks
	    tassert(localData.switching() == 1,
		    err_printf("Didn't grab sw bit\n"));
	    tassert(localData.destroying() == 0,
		    err_printf("oops: destroying bit set\n"));

	    if (localData.pendingDestroy()) rtn=DESTROYREQUIRED;
	    // OK PRECONDITONS MET
	    localData.switching(0);  // reset switching bit
	    success = CompareAndStore64Synced(&gteData.data,
					      olocalData.data, localData.data);
	} while (!success); // fails if something has changed must rety
	return rtn;
    }
};

struct LTransEntry : TransEntry {
    void setToDefault(COSDefaultObject *dobj) {
	tobj = (COSTransObject *)*(COSTransObject **)dobj;
	co   = (COSTransObject *)&tobj;
    }
    void setCO(COSTransObject *cobj) { co = cobj; }

};
#endif /* #ifndef IN_C */



#endif /* #ifndef __TRANS_ENTRY_H_ */
