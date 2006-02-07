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
 * $Id: ServerObject.C,v 1.4 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "ServerObject.H"
#include "KFSGlobals.H"
#include "SuperBlock.H"
#include "FSFileKFS.H"

/*
 *  ServerObject
 *      constructors
 */
ServerObject::ServerObject(KFSGlobals *g)
{
    lock.init();
    globals = g;
    recordMap = NULL;
    flags = 0;
    id.id = 0;
}

ServerObject::ServerObject(ObjTokenID *otokID, RecordMapBase *r,
			   KFSGlobals *g) : id(*otokID)
{
    lock.init();
    globals = g;
    recordMap = r;
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
		"ServerObject::~ServerObject() id=%llu IN\n", id.id);

    destructionTask();

    if (id.id) {
	if (!recordMap->removeObj(&id)) {
	    passertMsg(0, "~ServerObject: object not found int hash!\n");
	}
    }

    KFS_DPRINTF(DebugMask::SERVER_OBJECT,
		"ServerObject::~ServerObject() id=%llu OUT\n", id.id);
}

/* virtual */ RecordMapBase *
ServerObject::getRecordMap()
{
    if (recordMap) {
	return recordMap;
    }

    passertMsg(0, "ServerObject::getRecordMap() NULL recordMap!\n");
    return NULL;
}

SysStatusUval
ServerObject::isDirty()
{
    return (flags & SERVER_OBJECT_DIRTY);
}

/* virtual */ void
ServerObject::markDirty()
{
    flags |= SERVER_OBJECT_DIRTY;
    postMarkDirty();
}

/* virtual */ void
ServerObject::markClean()
{
    flags |= ~SERVER_OBJECT_DIRTY;
    postMarkClean();
}

/*
 * alloc()
 *
 *   Returns a newly allocated server object of the requested type.
 */
ServerObject *
ServerObjectAllocator::alloc(ObjTokenID *otokID, RecordMapBase *r, PsoType fileType)
{
    if (obj[fileType] == NULL) {
	err_printf("(ServerObjectAllocator::alloc) entry.fileType=%u\n",
		   fileType);
        return NULL;
    }

    return obj[fileType]->clone(otokID, r);
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
    if(obj[type] != NULL) {
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
ServerObjectAllocator::locationAlloc(PsoType type, ObjTokenID *otokID,
				     RecordMapBase *r)
{
    if ((uval)type > maxTypes) {
        return 0;
    }

    if (obj[type] == NULL) {
        return 0;
    }
    return obj[type]->locationAlloc(otokID, r);
}
