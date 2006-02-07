/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Factory.C,v 1.5 2005/08/09 12:03:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base class for all type factories
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/Callback.H>
#include <misc/simpleThread.H>
#include <cobj/DTType.H>
#include <cobj/sys/COSMgrObject.H>
#include <sync/atomic.h>
#include "Factory.H"

/* virtual */ SysStatus
Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    tassert(0, err_printf("must be overridden by factory capable of update\n"));
    return -1;
}

class BaseFactory::DataTransferObj : public DataTransferObject {
protected:
    InstanceList *instanceList;
    FactoryRef     replacingRef;
public:
    DEFINE_LOCALSTRICT_NEW(DataTransferObj);
    DataTransferObj(InstanceList *list, FactoryRef ref) : 
        instanceList(list), replacingRef(ref) { /* empty */ }
    InstanceList *getInstanceList() { return instanceList; }
    FactoryRef    getReplacingRef() { return replacingRef; }
};

void 
BaseFactory::Root::init() 
{
    updateThreadCount = 0;
    // for the sake of catching bugs we zero the repArray
    memset((void *)repArray, 0, sizeof(repArray)); 
}

void
BaseFactory::Root::recordRep(BaseFactory *rep, VPNum vp) {
    tassertMsg(repArray[vp]==NULL, "trying to record %p but %p already"
               " recorded for vp=%ld\n",rep, repArray[vp], vp);
    repArray[vp] = rep;
}

BaseFactory *
BaseFactory::Root::lookupRep(VPNum vp) {
    return repArray[vp];
}

/* virtual */ SysStatus
BaseFactory::Root::getDataTransferExportSet(DTTypeSet *set)
{
    set->addType(DTT_FACTORY_DEFAULT);
    return 0;
}

/* virtual */ SysStatus
BaseFactory::Root::getDataTransferImportSet(DTTypeSet *set)
{
    set->addType(DTT_FACTORY_DEFAULT);
    return 0;
}


/* virtual */ DataTransferObject *
BaseFactory::Root::dataTransferExport(DTType dtt, VPSet transferVPSet)
{
    BaseFactory *rep = (BaseFactory *)getRepOnThisVP();;
    InstanceList *instanceList;

    tassert(dtt == DTT_FACTORY_DEFAULT, err_printf("wrong transfer type\n"));

    /* create new instance list by merging old and new lists */
    instanceList = rep->oldInstanceList;
    rep->oldInstanceList = NULL;
    if (instanceList == NULL) {
        instanceList = new InstanceList();
    }
    /* don't need locks, since we are quiescent */
    rep->instanceList.locked_transferTo(*instanceList);

    return new DataTransferObj(instanceList, (FactoryRef)getRef());
}

/* virtual */ SysStatus
BaseFactory::Root::dataTransferImport(DataTransferObject *dtobj,
                                        DTType dtt, VPSet transferVPSet)
{
    SysStatus rc;
    BaseFactory *rep;
    FactoryRef   replacingRef;                                  
    tassert(dtt == DTT_FACTORY_DEFAULT, err_printf("wrong transfer type\n"));

    rep = (BaseFactory *)getRepOnThisVP();
    rep->oldInstanceList = ((DataTransferObj *)dtobj)->getInstanceList();
    replacingRef = ((DataTransferObj *)dtobj)->getReplacingRef();
    delete dtobj;
    
    if (myRef != (CORef) replacingRef) { 
        tassertMsg(myStaticRefPtr != NULL, "myStaticRefPtr==NULL\n");

        /* Looks like we are the kludgy attempt of taking over a */
        /* Factory after we have already been created, since     */
        /* I don't trust keeping two Refs for a CO we give up    */
        /* our original Ref using more code I don't trust.       */
        /* We rely on RCU for this to be safe.                   */
        VPNum vp=Scheduler::GetVP();                            
        /* Simply swing our Class Ref and let RCU mechanisms  */
        /* take care of in flight calls                       */
        if (vp==0) {                                            
            // swing my static ref pointer.... this all works because of RCU???
            // Two cases :
            //   Destroy of instance in flight:These may have cached the oldref
            //     from a DREF_FACTORY_DEFAULT(MyClass)... but that is ok since
            //        our mapping on that ref is stable until the reclaimRef
            //        action occurs which will only happen after this these 
            //        these threads terminate.
            //   Destory of instance happens after we swing the
            //      DREF_FACTORY_DEFAULT(MyClass) value but before we have 
            //      completed the swap... these calls will
            //      operate safely given the swap logic.
            //   
            *myStaticRefPtr=replacingRef;
            DREFGOBJ(TheCOSMgrRef)->reclaimRef(myRef, this);     
            /* FIXME: Not sure this is actually safe.             */
            /*        Think hard about this but not today         */
            myRef = (CORef)replacingRef;                               
        }                   
    }               

#if 1 /* HACK for dynupdate, start the update automatically on swap */
    rc = rep->updateOldInstances();
    tassertMsg(_SUCCESS(rc), "updateOldInstances failed\n");
#endif
    return 0;
}

/* virtual */ BaseFactory *
BaseFactory::lookupRep(CORef ref)
{
    BaseFactory *rep;
    VPNum vp = COSMgrObject::refToVP(ref);

    rep = COGLOBAL(lookupRep(vp));
    tassertMsg(rep, "no factory rep for ref=%p vp=%ld\n", ref, vp);

    return rep;
}

/* virtual */ SysStatus
BaseFactory::deregisterInstance(CORef ref)
{
    BaseFactory *rep = lookupRep(ref);

    /* FIXME: this is going to perform terribly */
    if (rep->instanceList.remove(ref) || rep->oldInstanceList->remove(ref)) {
        return 0;
    } else {
        err_printf("Warning: deregisterInstance(%p) failed\n", ref);
        return -1; /* FIXME */
    }
}

class BaseFactory::SwitchCallback : public Callback {
    FactoryRef factory;
    CORef ref;

public:
    virtual void complete(SysStatus success)
    {
	DREF(factory)->switchCompleteCallback(ref, success);
	delete this;
    }

    DEFINE_GLOBAL_NEW(SwitchCallback); /* FIXME: could we use LOCALSTRICT? */
    SwitchCallback(FactoryRef f, CORef r) : factory(f), ref(r) {}
};

/* static */ SysStatus
BaseFactory::updateThreadFunc(void *arg)
{
    CORef ref;
    SysStatus rc = 0;
    CObjRoot *root;
    void *iter;
    SwitchCallback *cb;
    BaseFactory *rep = (BaseFactory *)arg;
    FactoryRef myRef = (FactoryRef)rep->getRef();

    if (rep->oldInstanceList != NULL) {
        AtomicAdd(&rep->root()->updateThreadCount, 1);
        iter = NULL;
        while ((iter = rep->oldInstanceList->next(iter, ref))) {
            rc = rep->createReplacement(ref, root);
            if (_FAILURE(rc)) {
                tassertMsg(0, "createReplacement failed: %ld\n", rc);
                break;
            }
	    cb = new SwitchCallback(myRef, ref);
            rc = COSMgr::switchCObj(ref, root, COSMgr::DESTROY_WHEN_DONE,
				    1, cb);
            if (_FAILURE(rc)) {
                tassertMsg(0, "switchCObj failed: %ld\n", rc);
                break;
            }
        }
    }

    return rc;
}

/* virtual */ SysStatus
BaseFactory::updateOldInstances()
{
    SimpleThread *threads[Scheduler::VPLimit];
    SysStatus retval = 0, rc;
    uval vp, i, numThreads = 0;
    void *curr = NULL;
    CObjRep *rep;

    while ((curr = root()->nextRepVP(curr, vp, rep)) != NULL) {
        tassert(numThreads < Scheduler::VPLimit, err_printf("too many reps\n"));
        threads[numThreads] = SimpleThread::Create(updateThreadFunc, rep,
                                                   SysTypes::DSPID(0, vp));
        passert(threads[numThreads] != NULL,
                err_printf("SimpleThread::Create failed\n"));
        numThreads++;
    }

    for (i = 0; i < numThreads; i++) {
        rc = SimpleThread::Join(threads[i]);
        retval = retval ? retval : rc;
    }

    return retval;
}

/* virtual */ SysStatus
BaseFactory::printInstances()
{
    BaseFactory *rep=0;
    uval i;
    CORef instanceRef;
    void *iter;

    COGLOBAL(lockReps());

    for (void *curr=COGLOBAL(nextRep(0,(CObjRep *&)rep));
         curr; curr=COGLOBAL(nextRep(curr,(CObjRep *&)rep))) {
        iter=NULL;
        i=0;
        while ((iter=rep->instanceList.next(iter,instanceRef))) {
            err_printf("  rep=%p instance %ld : %p\n", rep, i, instanceRef);
            i++;
        }
        if (rep->oldInstanceList != NULL) {
            iter=NULL;
            i=0;
            while ((iter=rep->oldInstanceList->next(iter, instanceRef))) {
                err_printf("  rep=%p old instance %ld : %p\n", rep, i,
                           instanceRef);
                i++;
            }
        }
    }

    COGLOBAL(unlockReps());
    return 0;
}

/* virtual */ void
BaseFactory::switchCompleteCallback(CORef instance, SysStatus success)
{
    BaseFactory *rep;

    tassertMsg(_SUCCESS(success), "BaseFactory: switch failed %lx\n", success);

    rep = lookupRep(instance);
    if (rep->oldInstanceList->remove(instance) == 0) {
        tassertMsg(0, "how did we get a switch callback for this instance?\n");
        return;
    }

    if (rep->oldInstanceList->isEmpty()) {
    	// all the updates for this rep have completed
        delete rep->oldInstanceList;
        rep->oldInstanceList = NULL;

        if (FetchAndAddSigned((sval *) &COGLOBAL(updateThreadCount), -1) == 1) {
	    // all the updates for the whole factory have completed!
            if (COGLOBAL(updateCallback) != NULL) {
                COGLOBAL(updateCallback)->complete(0);

		// make sure we don't call this twice, as it might delete itself
		// updateOldInstances() should only happen once anyway
		COGLOBAL(updateCallback) = NULL;
            }
        }
    }
}

/* virtual */
BaseFactory::~BaseFactory()
{
    tassertMsg((oldInstanceList == NULL || oldInstanceList->isEmpty()) &&
               instanceList.isEmpty(), "factory being destroyed prematurely\n");
    if (oldInstanceList != NULL) {
        delete oldInstanceList;
    }
}
