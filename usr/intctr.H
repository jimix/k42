#ifndef __INTCTR_H_
#define __INTCTR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: intctr.H,v 1.16 2002/10/10 13:09:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for dyn-switch.
 * **************************************************************************/

// Integer counter clustered object classes

#include <cobj/CObjRootSingleRep.H>
#include <cobj/CObjRootMultiRep.H>
#include <cobj/DTType.H>
#include <scheduler/Scheduler.H>

extern sval err_tprintf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

class IntCtr : public BaseObj {
public:
    virtual SysStatus inc()=0;
    virtual SysStatus dec()=0;
    virtual SysStatus value(sval &count)=0;
};


typedef IntCtr **IntCtrRef;

class SharedIntCtrRoot;
class PartitionedIntCtrRoot;

class SharedIntCtrRoot : public CObjRootSingleRep {

    friend class PartitionedIntCtrRoot;

    typedef SharedIntCtrRoot Root;

    DEFINE_GLOBAL_NEW(SharedIntCtrRoot);
    SharedIntCtrRoot(BaseObj *co) : CObjRootSingleRep(co) { /* empty body */ }
    SharedIntCtrRoot(BaseObj *co, RepRef ref,
		     CObjRoot::InstallDirective idir=CObjRoot::Install) :
	CObjRootSingleRep(co, ref, idir) {
	/* empty body */
    }

public:
    // Data-transfer-related methods
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set) {
	// Assumes initialized state of set is correct
	set->addType(DTT_COUNTER_CANONICAL_VAL);
	set->addType(DTT_COUNTER_CANONICAL_REF);
	return 0;
    }
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set) {
	// Assumes initialized state of set is correct
	set->addType(DTT_COUNTER_CANONICAL_VAL);
	set->addType(DTT_COUNTER_CANONICAL_REF);
	set->addType(DTT_COUNTER_PARTITIONED_VAL);
	set->addType(DTT_COUNTER_PARTITIONED_REF);
	return 0;
    }
    virtual DataTransferObject *dataTransferExport(DTType dtt, VPSet dtVPSet);
    virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					 DTType dtt, VPSet dtVPSet);
    static IntCtrRef Create(BaseObj *co, void *ref=NULL,
			    uval install = 1) {
	CObjRoot *root;
	if (ref == NULL) {
	    root = new Root(co);
	} else {
	    root = new Root(co, (RepRef)ref,
		    install ? CObjRoot::Install : CObjRoot::skipInstall);
	}

	return (IntCtrRef)root->getRef();
    }
    static SysStatus SwitchImpl(CORef ref, uval id);
#if 0
    static SysStatus rootXferSame(CObjRoot *oldRoot, CObjRoot *newRoot);
    static SysStatus rootXferToP(CObjRoot *oldRoot, CObjRoot *newRoot);
#endif /* #if 0 */
};

class SharedIntCtr : public IntCtr {

    friend class SharedIntCtrRoot;
    friend class PartitionedIntCtrRoot;

    typedef SharedIntCtrRoot Root;

protected:
    sval _value;
    BLock lock;
    DEFINE_GLOBAL_NEW(SharedIntCtr);
public:
    SharedIntCtr() : _value(0) { /* empty body */ }
    virtual SysStatus inc();
    virtual SysStatus dec();
    virtual SysStatus value(sval &count)
    {
	count = _value;
	return 0;
    }
    static SysStatus Create(IntCtrRef &ref)
    {
	ref = (IntCtrRef)Root::Create(new SharedIntCtr);
	return 0;
    }
};

// -----------------

class PartitionedIntCtrRoot : public CObjRootMultiRep {
    // Grant full access to those roots that will switch to me.
    // Should be enough, in theory, to grant access only to rootTransfer.
    // But we probably need access to the rep class also.
    friend class SharedIntCtrRoot;

    typedef PartitionedIntCtrRoot Root;

    DEFINE_GLOBAL_NEW(PartitionedIntCtrRoot);
    PartitionedIntCtrRoot() { /* empty body */ }
    PartitionedIntCtrRoot(RepRef ref, sval c=1,
	 CObjRoot::InstallDirective idir=CObjRoot::Install) :
	CObjRootMultiRep(ref, c, idir) {
	/* empty body */
    }

protected:
public:
    virtual CObjRep *createRep(VPNum vp);
    // Data-transfer-related methods
    virtual SysStatus  getDataTransferExportSet(DTTypeSet *set) {
        // can cache the result
	// can comment out the highest priority one to test the next highest
	set->addType(DTT_COUNTER_CANONICAL_VAL);
	set->addType(DTT_COUNTER_PARTITIONED_VAL);
	set->addType(DTT_COUNTER_PARTITIONED_REF);
	return 0;
    }
    virtual SysStatus  getDataTransferImportSet(DTTypeSet *set) {
	// can cache the result
	set->addType(DTT_COUNTER_CANONICAL_VAL);
	set->addType(DTT_COUNTER_CANONICAL_REF);
	set->addType(DTT_COUNTER_PARTITIONED_VAL);
	set->addType(DTT_COUNTER_PARTITIONED_REF);
	return 0;
    }
    virtual DataTransferObject *dataTransferExport(DTType dtt, VPSet dtVPSet);
    virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					 DTType dtt, VPSet dtVPSet);
    static IntCtrRef Create() {
	Root *root = new Root();

	return (IntCtrRef)root->getRef();
    }

    static SysStatus SwitchImpl(CORef ref, uval id);
#if 0
    static SysStatus rootXferSame(CObjRoot *oldRoot, CObjRoot *newRoot);
    static SysStatus rootXferToS(CObjRoot *oldRoot, CObjRoot *newRoot);
#endif /* #if 0 */
};

class PartitionedIntCtr : public IntCtr {

    friend class PartitionedIntCtrRoot;
    friend class SharedIntCtrRoot;

public: // used in usr/dcotest.C
    typedef PartitionedIntCtrRoot Root;

    Root *root() { return (Root *)myRoot; }

protected:
    sval _localvalue;
    DEFINE_LOCALSTRICT_NEW(PartitionedIntCtr);
    //DEFINE_GLOBALPADDED_NEW(PartitionedIntCtr);
public:
    PartitionedIntCtr() : _localvalue(0) { /* empty body */ }
    virtual SysStatus inc()
    {
	_localvalue++;
	return 0;
    }
    virtual SysStatus dec()
    {
	_localvalue--;
	return 0;
    }
    virtual SysStatus value(sval &count)
    {
	PartitionedIntCtr *rep = 0;
	count = 0;
	root()->lockReps();
	for (void *curr=root()->nextRep(0,(CObjRep *&)rep);
	     curr; curr=root()->nextRep(curr,(CObjRep *&)rep)) {
	    count+=rep->_localvalue;
	}
	root()->unlockReps();
	return 0;
    }
    static SysStatus Create(IntCtrRef &ref)
    {
	ref = (IntCtrRef)Root::Create();
	return 0;
    }
};

#endif /* #ifndef __INTCTR_H_ */
