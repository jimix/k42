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
 * $Id: ObjToken.C,v 1.35 2004/03/07 00:42:39 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "ObjToken.H"
#include "ServerObject.H"
#include "KFSGlobals.H"
#include "RecordMap.H"

/*
 * ObjToken()
 *
 *   Null constructor to set the id to be invalid.
 */
ObjToken::ObjToken(KFSGlobals *g)
{
    id.id = 0;
    obj = NULL;
    globals = g;

    KFS_DPRINTF(DebugMask::OBJ_TOKEN, "Created empty object token\n");
}

/*
 * ObjToken(ID)
 *
 *   Sets the id to the requested id value.
 */
ObjToken::ObjToken(ObjTokenID ID, KFSGlobals *g) : id(ID) {
    obj = NULL;
    globals = g;
    KFS_DPRINTF(DebugMask::OBJ_TOKEN,
		"Created object token [%u]\n", id.id);
}

/*
 * ~ObjToken()
 *
 *   Destructor must release any referenced server objects
 */
ObjToken::~ObjToken() {
    KFS_DPRINTF(DebugMask::OBJ_TOKEN,
		"ObjToken::~ObjToken() executing for %p, id=%u\n",
		this, id.id);
    obj = (ServerObject *)0xDEADBEEF;
}

/*
 * setID()
 *
 *   Sets the token's id to the given id.
 */
void
ObjToken::setID(ObjTokenID ID)
{
    // make sure we release any object we might have now
    obj = NULL;

    // reset the id
    id = ID;
}

/*
 * getObj()
 *
 *   Returns the ServerObject associated with this token.  If it is
 *   not currently in memory, it allocates space and reads it off the
 *   disk.
 */
ServerObject *
ObjToken::getObj(FSFileKFS *f)
{
    PsoType type;

    // make sure this is a valid object token
    if (id.id == 0) {
	//tassertMsg(0, "look\n");
        return NULL;
    }

    // check if we already have a pointer
    if (obj == NULL) {
        // locate the appropriate Server Object or create a new one
	KFSHashEntry<ServerObject*> *entry;
	uval ret = globals->soHash.findAddLock(id.id, &entry);
	if (ret) {
	    obj = entry->getData();
	    tassertMsg(obj != NULL, "?");
	}
        if (ret == 0) {
#if 1 // for debugging
	    if (obj != NULL) {
		err_printf("\nFOUND obj, but from wrong disk\n");
	    }
#endif // #if 1 // for debugging            // read the proper entry
            if ((type = globals->recordMap->getRecordType(id)) <= 0) {
		passertMsg(0, "ObjToken::getObj() Problem getting type id %ld, "
			   "type %ld\n", (uval)id.id, (sval) type);
	    }

            // get a fresh ServerObject for this entry
            obj = globals->soAlloc->alloc(id, f, type);
	    passertMsg(obj != NULL, "obj NULL?");
	    entry->setData(obj);
        }

	entry->unlock();
    }

    return obj;
}
