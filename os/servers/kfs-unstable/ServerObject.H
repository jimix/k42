#ifndef __SERVER_OBJECT_H_
#define __SERVER_OBJECT_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: ServerObject.H,v 1.3 2004/05/05 19:58:00 lbsoares Exp $
 *****************************************************************************/

#include "ObjToken.H"
#include "RecordMapBase.H"
#include "KFSGlobals.H"

#define SERVER_OBJECT_DIRTY 1

/*
 * The ServerObject class is the parent class of both physical and
 * logical server objects (PSO and LSO respectively).
 */
class ServerObject {
private:
    // object tokens pointing to this ServerObject to update on deletion
//    ListSimpleLocked<ObjToken *, AllocGlobal> objTokenList;

protected:
    KFSGlobals *globals;

    // the object id number of this Server Object
    ObjTokenID id;

    // the RecordMap this ServerObject belongs to
    RecordMapBase *recordMap;

    // the lock for this object
    BLock lock;

    // currently used for dirtyness
    uval32 flags;

    // a class of specializations may need a common task to be performed
    // on ServerObject destruction and operations such as markDirty and
    // markClean. (an example is the scheme
    // we're currently working on for keeping a consistent view of the fs
    // on the disk). This is a place holder
    virtual void destructionTask() {}
    virtual void postMarkDirty() {}
    virtual void postMarkClean() {}
public:
    DEFINE_GLOBAL_NEW(ServerObject);

    // constructors
    ServerObject(KFSGlobals *g);
    ServerObject(ObjTokenID *otokID, RecordMapBase *r, KFSGlobals *g);

    virtual ~ServerObject();

    // returns the id value
    void setID(ObjTokenID *ID) { id = *ID;}
    ObjTokenID getID() { return id; }

    // creates a new ServerObject with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID *id, RecordMapBase *r) = 0;

    // allocates disk space for the ServerObject and returns its location
    virtual SysStatusUval locationAlloc(ObjTokenID *id, RecordMapBase *) = 0;

    // I think these shouldn't be virtual...
    RecordMapBase *getRecordMap();
    void setRecordMap(RecordMapBase *r) {
	passertMsg(!recordMap || recordMap == r, "ServerObject::setRecordMap() "
		   "recordMap! current=0x%p, new=0x%p\n", recordMap, r);
	recordMap = r;
    }

    // checks if there are any pending differences between memory and disk
    SysStatusUval isDirty();
    void markDirty();
    void markClean();

    virtual void flush() = 0;
};

/*
 * The ServerObjectAllocator is used to alloc() new ServerObjects.
 * It maintains a generic server object of each type.  When a new
 * object of a particular type is requested, the result of the clone()
 * function is returned to the caller.  New types can be registered
 * with the allocator through the join() call.
 */
class ServerObjectAllocator {
private:
    static const uval maxTypes = 256;
    ServerObject *obj[maxTypes];

public:
    DEFINE_GLOBAL_NEW(ServerObjectAllocator);
    ServerObjectAllocator() {
	memset(obj, 0, sizeof(ServerObject *)*maxTypes);
    }
    ~ServerObjectAllocator() {
	for (uval i = 0; i < maxTypes; i++) {
	    if (obj[i]) {
		delete obj[i];
	    }
	}
    }
    ServerObject *alloc(ObjTokenID *otokID, RecordMapBase *r, PsoType t);
    sval join(ServerObject *, uval);
    SysStatusUval locationAlloc(PsoType, ObjTokenID *otokID, RecordMapBase *);
};

#endif /* #ifndef __SERVER_OBJECT_H_ */
