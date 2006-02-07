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
 * $Id: ServerObject.H,v 1.34 2005/07/05 16:52:00 dilma Exp $
 *****************************************************************************/

#include "ObjToken.H"
#include "KFSGlobals.H"

class FSFileKFS;

#define SERVER_OBJECT_DIRTY 1

/*
 * The ServerObject class is the parent class of both physical and
 * logical server objects (PSO and LSO respectively).
 */
class ServerObject {
public:
    struct DirtySONode {
	BLock lock;
	ServerObject *so;
	uval valid;
	DEFINE_GLOBAL_NEW(DirtySONode);
	DirtySONode(ServerObject *s) : so(s), valid(1) { lock.init(); }
	~DirtySONode() {}
	uval locked_isValid() { _ASSERT_HELD(lock); return valid; }
	void invalidate() { 
	    AutoLock<BLock> al(&lock); // locks now, unlocks on return
	    valid = 0; 
	}
    };


private:
    // object tokens pointing to this ServerObject to update on deletion
//    ListSimpleLocked<ObjToken *, AllocGlobal> objTokenList;

protected:
    KFSGlobals *globals;

    // the object id number of this Server Object
    ObjTokenID id;

    // the FSFile this ServerObject belongs to
    FSFileKFS *fsfile;

    // the lock for this object
    BLock lock;

    // currently used for dirtyness
    uval32 flags;

    // The SuperBlock needs to keep a list of dirty objects (so they can
    // be flushed periodically). Keeping ServerObject pointers in the
    // SuperBlock would require a lookup/removal on the ServerObject
    // removal path (therefore the list would have to be a hash ...).
    // Instead, we have the list to keep nodes (SuperBlock::DirtySONode)
    // which cointains the ServerObject pointer and isValid() information.
    // The destruction of ServerObjects invokes dirtyNode->invalidate().
    DirtySONode *dirtyNode;

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
    ServerObject(ObjTokenID id, FSFileKFS *fsfile, KFSGlobals *g);

    virtual ~ServerObject();

    // returns the id value
    void setID(ObjTokenID ID) { id = ID;}
    ObjTokenID getID() { return id; }

    FSFileKFS *getFSFile() { return fsfile; }

    DirtySONode *getDirtyNode() { return dirtyNode; }
    void setDirtyNode(DirtySONode *node) { dirtyNode = node; }

    // creates a new ServerObject with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID id, FSFileKFS *f) = 0;

    // allocates disk space for the ServerObject and returns its location
    virtual SysStatusUval locationAlloc(ObjTokenID) = 0;

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
    ServerObject *alloc(ObjTokenID ID, FSFileKFS *f, PsoType t);
    sval join(ServerObject *, uval);
    SysStatusUval locationAlloc(PsoType, ObjTokenID);
};

#endif /* #ifndef __SERVER_OBJECT_H_ */
