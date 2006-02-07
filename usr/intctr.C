/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: intctr.C,v 1.8 2002/11/22 13:53:56 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for dyn-switch.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stddef.h>
#include "intctr.H"
#include "dcoXferObj.H"

/*virtual*/ CObjRep *
PartitionedIntCtrRoot::createRep(VPNum vp) {
    CObjRep *rep = (CObjRep *)new PartitionedIntCtr;
    return rep;
}

#if 0
/*static*/ SysStatus
SharedIntCtrRoot::rootXferSame(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    err_tprintf("SharedIntCtr::Root::rootXferSame()\n");

    // ``Data transfer''
    SharedIntCtr *const rep = (SharedIntCtr *)newRoot->getRepOnThisVP();
    ((SharedIntCtr *)oldRoot->getRepOnThisVP())->value(rep->_value);
    err_tprintf("value = %ld\n", rep->_value);

    return 0;
}

/*static*/ SysStatus
SharedIntCtrRoot::rootXferToP(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    err_tprintf("SharedIntCtr::Root::rootXferToP()\n");

    // ``Data transfer''
    PartitionedIntCtr *const rep =
	(PartitionedIntCtr *)newRoot->getRepOnThisVP();
    ((SharedIntCtr *)oldRoot->getRepOnThisVP())->value(rep->_localvalue);
    err_tprintf("value = %ld\n", rep->_localvalue);

    return 0;
}
#endif

/*static*/ SysStatus
SharedIntCtrRoot::SwitchImpl(CORef ref, uval id)
{
    //err_tprintf("PartitionedIntCtr::Root::SwitchImpl()\n");

    if (id == 0) {
	SharedIntCtr *rep = new SharedIntCtr;
	CObjRoot *nroot = new Root(rep, (RepRef)ref, CObjRoot::skipInstall);
	return COSMgr::switchCObj(ref, nroot/*, rootXferSame*/);
    } else {
	PartitionedIntCtrRoot *nroot =
	    new PartitionedIntCtrRoot((RepRef)ref, 1, CObjRoot::skipInstall);
	return COSMgr::switchCObj(ref, nroot/*, rootXferToP*/);
    }
}

/*virtual*/ DataTransferObject *
SharedIntCtrRoot::dataTransferExport(DTType dtt, VPSet dtVPSet)
{
    DataTransferObject *data = 0;

    switch (dtt) {
    case DTT_COUNTER_CANONICAL_VAL:
	data = new DataTransferObjectCounter(((SharedIntCtr *)therep)->_value);
	//err_printf("EXPORT: value = %ld\n", data->value());
	break;
    case DTT_COUNTER_CANONICAL_REF:
	data = new DataTransferObjectCounterPtr(&((SharedIntCtr
			*)therep)->_value);
	break;
    default:
	tassert(0, err_printf("ShareCounter:export: unsupported type!\n"));
	break;
    }
    return (DataTransferObject *)data;
}

/*virtual*/ SysStatus
SharedIntCtrRoot::dataTransferImport(DataTransferObject *dtobj, DTType dtt,
				     VPSet dtVPSet)
{
    if (dtobj) {
	DataTransferObjectCounter *c = (DataTransferObjectCounter *)dtobj;
	DataTransferObjectCounterPtr *cp =
	    (DataTransferObjectCounterPtr *)dtobj;
	switch (dtt) {
	case DTT_COUNTER_CANONICAL_VAL:
	    if (dtVPSet.firstVP() == Scheduler::GetVP()) {
		((SharedIntCtr *)therep)->_value = c->value();
	    }
	    delete c;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_CANONICAL_REF:
	    if (dtVPSet.firstVP() == Scheduler::GetVP()) {
		((SharedIntCtr *)therep)->_value = *cp->ptr();
	    }
	    delete cp;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_PARTITIONED_VAL:
	    FetchAndAddSignedSynced(&((SharedIntCtr *)therep)->_value,
				  c->value());
	    //err_printf("IMPORT: value = %ld\n",
	      //  ((SharedIntCtr *)therep)->_value);
	    delete c;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_PARTITIONED_REF:
	    FetchAndAddSignedSynced(&((SharedIntCtr *)therep)->_value,
				  *cp->ptr());
	    //err_printf("IMPORT: value = %ld\n",
	      //  ((SharedIntCtr *)therep)->_value);
	    delete cp;	// FIXME: revise the reclamation protocol
	    break;
	default:
	    tassert(0, err_printf("ShareCounter:import: unsupported type!\n"));
	    break;
	}
    }
    err_printf("DT completed using typeid %ld.\n", (uval)dtt);
    return 0;
}

#if 0
/*static*/ SysStatus
PartitionedIntCtrRoot::rootXferSame(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    err_tprintf("PartitionedIntCtr::Root::rootXferSame()\n");

    // ``Data transfer''
    PartitionedIntCtr *const rep =
	(PartitionedIntCtr *)newRoot->getRepOnThisVP();
    ((PartitionedIntCtr *)oldRoot->getRepOnThisVP())->value(rep->_localvalue);
    err_tprintf("value = %ld\n", rep->_localvalue);

    return 0;
}

/*static*/ SysStatus
PartitionedIntCtrRoot::rootXferToS(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    //err_printf("PartitionedIntCtr::Root::rootXferToS()\n");

    // ``Data transfer''
    SharedIntCtr *const rep =
	(SharedIntCtr *)newRoot->getRepOnThisVP();
    ((PartitionedIntCtr *)oldRoot->getRepOnThisVP())->value(rep->_value);
    //err_tprintf("value = %ld\n", rep->_value);

    return 0;
}
#endif

/*static*/ SysStatus
PartitionedIntCtrRoot::SwitchImpl(CORef ref, uval id)
{
    err_tprintf("PartitionedIntCtr::Root::SwitchImpl()\n");

    if (id == 0) {
	Root *nroot = new Root((RepRef)ref, 1, CObjRoot::skipInstall);
	COSMgr::switchCObj(ref, nroot/*, rootXferSame*/);
	return 0;
    } else {
	SharedIntCtr *rep = new SharedIntCtr;
	CObjRoot *nroot = new SharedIntCtrRoot(rep, (RepRef)ref,
					       CObjRoot::skipInstall);
	return COSMgr::switchCObj(ref, nroot/*, rootXferToS*/);
    }
}

/*virtual*/ DataTransferObject *
PartitionedIntCtrRoot::dataTransferExport(DTType dtt, VPSet dtVPSet)
{
    sval val = 0;
    DataTransferObject *dtobj = 0;

    switch (dtt) {
    case DTT_COUNTER_CANONICAL_VAL:
	((PartitionedIntCtr *)getRepOnThisVP())->value(val);
	dtobj = (DataTransferObject *)new DataTransferObjectCounter(val);
	break;
    case DTT_COUNTER_PARTITIONED_VAL:
	val = ((PartitionedIntCtr *)getRepOnThisVP())->_localvalue;
	dtobj = (DataTransferObject *)new DataTransferObjectCounter(val);
	//err_printf("EXPORT: value = %ld\n", val);
	break;
    case DTT_COUNTER_PARTITIONED_REF:
	dtobj = (DataTransferObject *)new
	    DataTransferObjectCounterPtr(&(((PartitionedIntCtr *)
                                            getRepOnThisVP())->_localvalue));
	break;
    default:
	tassert(0, err_printf("PartitionedCounter:export: "
			      "unsupported type!\n"));
	break;
    }

    //err_printf("EXPORT: value = %ld\n", val);
    return dtobj;
}

/*virtual*/ SysStatus
PartitionedIntCtrRoot::dataTransferImport(DataTransferObject *dtobj,
					  DTType dtt, VPSet dtVPSet)
{
    VPNum myvp = Scheduler::GetVP();

    if (dtobj) {
	DataTransferObjectCounter *c = (DataTransferObjectCounter *)dtobj;
	DataTransferObjectCounterPtr *cp =
	    (DataTransferObjectCounterPtr *)dtobj;
	switch (dtt) {
	case DTT_COUNTER_CANONICAL_VAL:
	    if (dtVPSet.firstVP() == myvp) {
		((PartitionedIntCtr *)getRepOnThisVP())->_localvalue =
                    c->value();
	    }
	    delete c;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_CANONICAL_REF:
	    if (dtVPSet.firstVP() == myvp) {
		((PartitionedIntCtr *)getRepOnThisVP())->_localvalue =
                    *cp->ptr();
	    }
	    delete cp;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_PARTITIONED_VAL:
	    ((PartitionedIntCtr *)getRepOnThisVP())->_localvalue =
		c->value();
	    delete c;	// FIXME: revise the reclamation protocol
	    break;
	case DTT_COUNTER_PARTITIONED_REF:
	    ((PartitionedIntCtr *)getRepOnThisVP())->_localvalue =
		*cp->ptr();
	    delete cp;	// FIXME: revise the reclamation protocol
	    break;
	default:
	    tassert(0, err_printf("PartitionedCounter:import: "
				  "unsupported type!\n"));
	}
    }
    err_printf("DT completed using typeid %ld.\n", (uval)dtt);
    return 0;
}
