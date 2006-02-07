#ifndef __OBJ_TOKEN_H_
#define __OBJ_TOKEN_H_

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
 * $Id: ObjToken.H,v 1.22 2004/03/07 00:52:45 lbsoares Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *		  Hurricane File Manager : ObjectToken.h
 *			     Copyright 1989
 *
 *		 Authors: Orran Krieger
 *****************************************************************************/

#include "KFSDebug.H"
#include "PSOTypes.H"

// "define" class ServerObject since we include ObjToken.H in ServerObject.H
class ServerObject;
class Disk;
class KFSGlobals;
class FSFileKFS;

struct ObjTokenID {
    // FIXME: we really want id uval64 and dPart as uval8, but as I debug endian issues
    // let's keep this structure with size == 8 bytes, like it was before
    // the "identifier" of the object
    uval32 id;
};

/*
 * The ObjToken class is used to track objects within the file system.
 */
class ObjToken
{
    KFSGlobals *globals;
protected:
    // add the object id number here...
    ObjTokenID id;

    // pointer to corresponding ServerObject (PSO or LSO)
    ServerObject *obj;

public:
    // constructors
    DEFINE_GLOBAL_NEW(ObjToken);
    ObjToken(KFSGlobals *);
    ObjToken(ObjTokenID, KFSGlobals *);

    // destructor
    ~ObjToken();

    // set/get the object token's ID value
    void setID(ObjTokenID);
    ObjTokenID getID() { return id; }

    // returns the ServerObject for this object id
    ServerObject *gobj() { return obj; }
    ServerObject *getObj(FSFileKFS *f);

    // returns 1 if there is an object, 0 otherwise
    uval hasObj() {
        return (id.id == 0) ? 0 : 1;
    }
};

#endif /* #ifndef __OBJ_TOKEN_H_ */
