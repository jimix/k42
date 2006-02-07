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
 * $Id: ServerObject.C,v 1.29 2005/07/05 16:51:59 dilma Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "ServerObject.H"
#include "KFSGlobals.H"
#include "SuperBlock.H"

/*
 *  ServerObject
 *      constructors
 */
ServerObject::ServerObject(KFSGlobals *g) : dirtyNode(NULL)
{
    lock.init();
    globals = g;
    fsfile = NULL;
    flags = 0;
    id.id = 0;
}

ServerObject::ServerObject(ObjTokenID ID, FSFileKFS *f, KFSGlobals *g)
    : id(ID), dirtyNode(NULL)
{
    lock.init();
    globals = g;
    fsfile = f;
    flags = 0;
}

/*
 * ~ServerObject()
 *
 *   ServerObject destructor.
 */
ServerObject::~ServerObject()
{
    KFS_DPRINTF(DebugMask::SERVER_OBJECT,
		"ServerObject::~ServerObject() id=%u IN\n", id.id);

    if (dirtyNode) {
	dirtyNode->invalidate();
	dirtyNode = NULL;
    }

    destructionTask();

    if (id.id) {
	KFSHashEntry<ServerObject*> *entry;
	uval ret = globals->soHash.findLock(id.id, &entry);
	if (ret) {
	    globals->soHash.removeUnlock(entry);
	} else {
	    passertMsg(0, "shouldn't happen\n");
	}
    }
    KFS_DPRINTF(DebugMask::SERVER_OBJECT,
		"ServerObject::~ServerObject() id=%u OUT\n", id.id);
}

SysStatusUval
ServerObject::isDirty()
{
    return (flags & SERVER_OBJECT_DIRTY);
}

/* virtual */ void
ServerObject::markDirty()
{
    if (!isDirty()) {
	dirtyNode = globals->super->addDirtySO(this);
    } else {
	// It would be natural to assume that if the SO is dirty, it has
	// to be already in the dirtySOList kept by the super block.
	// But this is not the case. It may be that the element
	// has just been taken out of the list for flushing, and now it's
	// waiting to acquire the lock and do the flush. As it waits,
	// it's being marked as dirty once more.
    }

    flags |= SERVER_OBJECT_DIRTY;
    postMarkDirty();
}

/* virtual */ void
ServerObject::markClean()
{
    flags &= ~SERVER_OBJECT_DIRTY;
    postMarkClean();
}

/*
 * alloc()
 *
 *   Returns a newly allocated server object of the requested type.
 */
ServerObject *
ServerObjectAllocator::alloc(ObjTokenID otokID, FSFileKFS *f, PsoType fileType)
{
    if (obj[fileType] == NULL) {
	err_printf("(ServerObjectAllocator::alloc) entry.fileType=%u\n",
		   fileType);
        return NULL;
    }

    return obj[fileType]->clone(otokID, f);
}

/*
 * join()
 *
 *   Registers a new server object with the requested type number.
 */
sval
ServerObjectAllocator::join(ServerObject *newObj, uval type)
{
    if (type > maxTypes) {
	tassertMsg(0, "?");
        return -1;
    }

    // FIXME: this function should have a lock around this
    if (obj[type] != NULL) {
	tassertMsg(0, "?");
        return -1;
    }
    obj[type] = newObj;

    return 0;
}

/*
 * locationAlloc()
 *
 *   Allocates disk space for the requested Server Object type and
 *   returns its location.
 */
SysStatusUval
ServerObjectAllocator::locationAlloc(PsoType type, ObjTokenID id)
{
    if (obj[type] == NULL) {
        return 0;
    }
    return obj[type]->locationAlloc(id);
}
