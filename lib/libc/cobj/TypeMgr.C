/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TypeMgr.C,v 1.48 2005/04/15 17:39:33 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the KernelTypeMgr
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/ObjectRefs.H>
#include "TypeMgr.H"
#include "TypeFactory.H"
#include <stub/StubTypeMgrServer.H>
#include <meta/MetaTypeMgrServer.H>
#include <trace/traceMisc.h>
#include "CObjRootSingleRep.H"
#include <misc/ListSimpleLocked.H>

#define TYPE_MGR_HASHMASK (TYPE_MGR_HASHNUM - 1)
#define TYPE_MGR_INC_ALLOC 32
#define TSNUM (backupServer!=NULL)

/* given our hashfunction and assuming a typestring is pretty much always
 * of length 256 or less, the maximum hashvalue we can get is 256*256 = 2^16
 */

#define HASHIDX(id) (((id) ^ (id>>16)) & TYPE_MGR_HASHMASK)
#define HASHVAL(id) ((id) & 0x00FFFFFF)
#define CNTVAL(id)  ((id) >> 24)
#define TYPEID(hval,cnt)  (((cnt) << 24) | (hval))

/*
 * Return an object handle to the global type manager
 */
/* virtual */ SysStatus
TypeMgr::getTypeMgrOH(ObjectHandle& returnOh)
{
    returnOh = backupServer->getOH();
    return 0;
}

/*
 * Class initializer
 */
void
TypeMgr::ClassInit(VPNum vp, StubTypeMgrServer* backup)
{
    if (vp!=0) return;			// nothing to do on second proc

    TypeMgr *typeMgrPtr = new TypeMgr;

    // initialize the TypeMgr structure
    typeMgrPtr->lock.init();
    typeMgrPtr->freeEntries = NULL;
    for (uval i= 0; i<TYPE_MGR_HASHNUM; i++) {
	typeMgrPtr->hashtab[i] = NULL;
    }

    typeMgrPtr->backupServer = backup;

    // now register ourselves in the global object table
    new CObjRootSingleRep(typeMgrPtr, (RepRef)GOBJ(TheTypeMgrRef));
}

/*
 * Turn a type name and hash value into a new hash value
 */
TypeID
TypeMgr::hash(const char *name, uval hv)
{
    char *p = (char*)name;
    /* rotate the first 24 bits by 2 and xor the new character into it */

    while (*p) {
	hv <<= 3;
	hv = ((hv & 0xFF000000) >> 24) | (hv & 0x00FFFFFF);
	hv = hv ^ *p;
	++p;
    }
    return (hv);
}

/*
 * Search for and return the type entry with the given id
 */
TypeMgrEntry*
TypeMgr::locate(TypeID id)
{
    uval idx = HASHIDX(id);
    TypeMgrEntry *e = hashtab[idx];
    while (e) {
	if (e->id == id)
	    return (e);
	e = e->next;
    }
    return (NULL);
}

/*
 * Flush cached information so that the next request will go to backup
 * server if there is one
 */
SysStatus
TypeMgr::resetTypeHdlr(const TypeID id)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    TypeMgrEntry *e = locate(id);
    if (e != NULL) {
//	traceMiscStr(TRACE_MISC_TYPE_HDLR_REG, 4, 1, e->string.length),
//		  TSNUM, id, (uval64)oh.pid(), oh.xhandle(), e->string.name);
	e->oh.init();
    }
    return (0);
}

/*
 * Registers a local pointer for a given type as well as an object handle
 */
SysStatus
TypeMgr::registerTypeHdlr(TypeID id, ObjectHandle oh, uval localPtr)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    TypeMgrEntry *e = locate(id);
    if (e == NULL) {
	err_printf("TypeMgr[%d]::regTypeHdlr(%lx) not found\n",TSNUM,id);
	return (_SERROR(1477, 0, ENOENT)); // unknown type
    } else {
//	traceMiscStr(TRACE_MISC_TYPE_HDLR_REG, 4, 1, e->string.length,
//		  TSNUM, id, (uval64)oh.pid(), oh.xhandle(), e->string.name);
	e->oh = oh;
	e->localPtr = localPtr;
    }
    if (backupServer) {
	return (backupServer->_registerTypeHdlr(id,oh));
    }
    return (0);
}

/*
 * Returns the registered object handler back to the caller
 */
SysStatus
TypeMgr::getTypeHdlr(TypeID id, ObjectHandle &oh)
{
    lock.acquire();
    SysStatus rc;

    TypeMgrEntry *e = locate(id);
    if (e == NULL) {
	/* in order to wanna get the typeHdler one must have registered
	 * previously, typically automtically done by the stub generated code
	 * Hence we will not check the backup server and return an error
	 * immediately
	 */
	lock.release();
	return (_SERROR(1478, 0, ENOENT));
    }
    if (e->oh.invalid()) {
	/* we don't have a valid handler registered or cached.
	 * Hence check with the backup server to obtain the handler if
	 * indeed the backup has registered one
	 */
	if (backupServer) {
	    rc = backupServer->_getTypeHdlr(id,oh);
	    if (rc == 0) {
		e->oh = oh;
	    }
	} else {
	    // must release lock around assert, since could call console
	    // which could make type request to type system
	    lock.release();
	    rc = _SERROR(1479, 0, ENOENT); // unknown type
	    lock.acquire();
	}
	if (rc != 0) {
	    lock.release();
	    return (rc);
	}
    } else {
	oh = e->oh;
    }
    lock.release();
    return (0);
}

/*
 * Returns a local pointer to the associated meta object
 */
SysStatusUval
TypeMgr::getTypeLocalPtr(TypeID id)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return

    TypeMgrEntry *e = locate(id);
    if (e == NULL) {
	/* in order to wanna get the typeHdler one must have registered
	 * previously, typically automtically done by the stub generated code
	 * Hence we will not check the backup server and return an error
	 * immediately
	 */
	err_printf("TypeMgr[%d]::getTypeLocalPtr(id = %lx) not found\n",
		   TSNUM,id);
	return (_SERROR(1928, 0, ENOENT));
    }

    if (e->oh.invalid()) {
	return 0;
    }

    return e->localPtr;
}

/*
 * Registers a string name with the type manager
 */
SysStatus
TypeMgr::registerType(const TypeID parentId,
                      const char *name,
                      uval signature,
                      TypeID &id)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    SysStatus rc;
    uval slen;
    TypeMgrEntry *e;          // this entry
    TypeMgrEntry *pe = NULL;  // parents entry

    if (parentId != TYPEID_NONE) {
	pe = locate(parentId); // check for existence
	if (pe == NULL) {
	    return _SERROR(1480, 0, EINVAL);  // parentId is invalid
	}
    }
    /* Now try to locate this beast in our server, we assume there
     * is no hash collision, if so we go through some more elaborate
     * path.
     */
    uval hval = hash("/",parentId);  // add the name separator
    hval = hash(name,hval);
    e = hashtab[HASHIDX(hval)];
    uval maxcnt = 0; // maximum counter we got

    while (e) {
	if (HASHVAL(e->id) == hval) {
	    if (strcmp(e->string.name,name) == 0) break;
	    if (CNTVAL(e->id) > maxcnt) maxcnt = CNTVAL(e->id);
	}
	e = e->next;
    }
    if (e != NULL) {
	/* we found it, so we have nothing else to do but return the id */
	id = e->id;
	return (0);
    }

    /* can't find this name, so go and retrieve the typeid from the backup
     * server if one exists else generate a new entry based on above maxcnt
     */
    if (backupServer) {
	rc = backupServer->_registerType(parentId,name,signature,id);
	if (rc != 0) return (rc);
    } else {
	id = TYPEID(hval,maxcnt+1);
    }

    /* now create a local entry for this new type */

    if (freeEntries == NULL) {
	e = new TypeMgrEntry[TYPE_MGR_INC_ALLOC];
	memset(e,0,TYPE_MGR_INC_ALLOC*sizeof(TypeMgrEntry));
	for (uval i = 2; i < TYPE_MGR_INC_ALLOC; i++) {
	    e[i-1].next = &e[i];
	}
	e[TYPE_MGR_INC_ALLOC-1].next = NULL;
	freeEntries = &e[1];
    } else {
	e = freeEntries;
	freeEntries = e->next;
    }
    e->id = id;
    e->parent = pe;
    if (e->parent) {
        e->parent->childList.add(e);
    }
    e->oh.initWithCommID(0,0);
    e->string.length = strlen(name);
    slen = e->string.length + 1;
    // grow if necessary
    if (slen > e->string.size) {
	if (e->string.size != 0) {
	    freeGlobal(e->string.name, e->string.size);
	}
	e->string.size = ((e->string.size > slen) ? e->string.size : slen) * 2;
	e->string.name = (char *)allocGlobal(e->string.size);
    }
    memcpy(e->string.name, name, slen);
    hval = HASHIDX(hval);
    e->next = hashtab[hval];
    hashtab[hval] = e;

//    traceMiscStr(TRACE_MISC_TYPE_REG, 3, 1, e->string.length,
//		 TSNUM, id, hval, name);

    /* since we assume a universal hash function, we can safely assume
     * that the backup server will do the same stuff with respect to
     * inheritance as this local server
     */

    return (0);
}

/*
 * Determines if a given type ID is derived from a given parent ID
 */
SysStatus
TypeMgr::isDerived(const TypeID derivedId, const TypeID baseId)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    TypeMgrEntry *de = locate(derivedId);
    TypeMgrEntry *be = locate(baseId);

    if ((de == NULL) || (be == NULL)) {
        if (backupServer) {
  	    return (backupServer->_isDerived(derivedId, baseId));
        }
        else {
	    return (_SERROR(1481, 0, EINVAL));
        }
    }

    TypeMgrEntry *ptr = de;
    while (ptr && (ptr != be)) ptr = ptr->parent;
    return (ptr != NULL);
}

/*
 * Returns the type name given the type ID
 */
SysStatus
TypeMgr::typeName(__in const TypeID id,
		     __outbuf(*:buflen) char *buf,
		     __in const uval buflen)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    return locked_typeName(id, buf, buflen);
}

SysStatus
TypeMgr::locked_typeName(const TypeID id, char *buf, const uval buflen)
{
    TypeMgrEntry *e = locate(id);

    if (e == NULL) {
	return (backupServer->_typeName(id,buf,buflen));
    }

    TypeMgrEntry *chain[32]; // assume maximum of 32 depth chain
    uval idx = 0;
    uval remlen = buflen;
    while (e) { chain[idx] = e; idx++; e = e->parent; }
    buf[0] = '\0';
    while (idx > 0) {
	idx--;
	e = chain[idx];
	uval len = e->string.length + 1;
	if (len  >= remlen) {
	    /* not enough space in this buffer to hold the string */
	    return (_SERROR(1482, 0, ENOMEM));
	}
	buf[0] = '/';
	memcpy(buf + 1, e->string.name, len);
	buf    += len;
	remlen -= len;
    }
    return (0);
}

/*
 * Prints out a dump of the type manager's tree structure
 */
SysStatus
TypeMgr::dumpTree(uval global)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    if (backupServer && global) {
	return backupServer->_dumpTree();
    }

    cprintf("TypeMgr[%d]::dumpTree()\n",TSNUM);
    for (uval i=0; i < TYPE_MGR_HASHNUM; i++) {
	TypeMgrEntry *e = hashtab[i];
	while (e) {
	    char tname[128];
	    locked_typeName(e->id,tname,128);
	    cprintf("\tid=0x%lx [%ld] : <%lx:0x%lx> %s\n",
		    e->id,HASHIDX(e->id),e->oh.pid(),e->oh.xhandle(),tname);
	    e = e->next;
	}
    }
    return (0);
}

/*
 * Looks up the type ID given the type's string name.
 */
/* virtual */ SysStatus
TypeMgr::locateType(const char *name, TypeID &id)
{
    uval hval, maxcnt = 0;
    TypeMgrEntry *e;

    // convert the string to the hash value
    hval = hash("/", TYPEID_NONE);
    hval = hash(name, hval);
    e = hashtab[HASHIDX(hval)];

    // loop searching for the given string name
    while (e) {
	if (HASHVAL(e->id) == hval) {
	    if (strcmp(e->string.name,name) == 0) break;
	    if (CNTVAL(e->id) > maxcnt) maxcnt = CNTVAL(e->id);
	}
	e = e->next;
    }

    // check with the backup server if we don't have it here
    if (e == NULL) {
        if (backupServer) {
            return backupServer->_locateType(name, id);
        }

        id = 0;
        return -1;
    }

    // set the id and return
    id = e->id;
    return 0;
}

/*
 * Locate the name of a type given its ID number.
 */
/* virtual */ SysStatus
TypeMgr::locateName(const TypeID id, char *name, uval nameLen)
{
    uval length;
    TypeMgrEntry *e;

    e = locate(id);
    if (e == NULL) {
        if (backupServer) {
            return backupServer->_locateName(id, name, nameLen);
        }

        return -1;
    }

    // copy the string
    length = (nameLen > e->string.size) ? e->string.size : nameLen;
    memcpy(name, e->string.name, length);
    name[length] = 0;

    return 0;
}

/*
 * Returns wether or not a given type id exists in this type manager.
 */
SysStatusUval
TypeMgr::hasType(TypeID id) {
    if (locate(id)) {
        return 1;
    }

    return 0;
}

/*
 * Returns the parent ID of the given type ID.
 */
SysStatusUval
TypeMgr::locateParent(TypeID id) {
    TypeMgrEntry *e = locate(id);
    if (e == NULL) {
        return 0;
    }

    if (e->parent == NULL) {
        return TYPEID_NONE;
    }
    return e->parent->id;
}

/*
 * Returns the factory ID value of the given type.
 * FIXME: this is part of the non-shared-library hack
 */
SysStatusUval
TypeMgr::locateFactoryID(TypeID id) {
    TypeMgrEntry *e = locate(id);
    if (e == NULL || e->factory == NULL) {
        return 0;
    }

    return e->factory->getID();
}

/*
 * Registers a factory with a particular type
 */
SysStatus
TypeMgr::registerFactory(TypeID id, uval factoryID)
{
    // register the factory with the backup server
    if (backupServer) {
        backupServer->_registerFactory(id, factoryID);
    }

    TypeMgrEntry *e = locate(id);
    if (e == NULL) {
        return -1;
    }

    // check if we already have the factory
    if (e->factory != NULL) {
        return 0;
    }

    // set the factory
    e->factory = FactoryTable::getFact(factoryID);
    if (e->factory == NULL) {
        return -1;
    }

    e->factory->setEntry(e);
    return 0;
}

/*
 * Returns a pointer to the factory for this type, given the type name.
 */
SysStatusUval
TypeMgr::locateFactory(const char *_typeName)
{
    TypeID id;

    // locate the correct type
    locateType(_typeName, id);
    if (id == 0) {
        return 0;
    }

    return locateFactory(id);
}

/*
 * Returns a pointer to the factory for this type, given the type ID.
 */
SysStatusUval
TypeMgr::locateFactory(const TypeID id)
{
    uval tmpID;
    uval factoryID;
    TypeMgrEntry *e;
    char tName[256];
    // AutoLock<BLock> al(&lock); // locks now, unlocks on return

    e = locate(id);
    if (e == NULL || e->factory == NULL) {
        if ((backupServer != NULL) &&
            (backupServer->_hasType(id))) {

            // load the type into local type manager
            if (e == NULL) {
                uval parentID = backupServer->_locateParent(id);
                locateName(id, tName, 255);
                registerType(parentID, tName, 1, tmpID);
            }

            // FIXME: get the factory's shared-lib info, etc. and then
            //        return a generic mediator which first loads the
            //        shared library into the application, then swaps
            //        itself out and you have access to the factory?
            //        OR
            //        Maybe we should just call something which will
            //        do the loading of the shared library for us?

            // FIXME: call the backupServer and have it return the
            //        shared library name and symbol name, or some
            //        such thing

            // note: until shared libraries, we use a well-known
            //       index into a switch statement for creating
            //       the factories
            factoryID = (uval)backupServer->_locateFactoryID(id);
            registerFactory(id, factoryID);
            if (e == NULL) { e = locate(id); }
        } else {
            return 0;
        }
    }

    tassert(e->factory != NULL, err_printf("factory pointer is NULL!\n"));
    return (SysStatusUval)e->factory;
}

/*
 * Updates the given entry to have all of the registered children
 */
SysStatus
TypeMgr::updateChildren(const TypeID id)
{
    uval64 children[10];
    uval childCount;
    TypeID tmpID;
    char name[1024];

    // get a list of each child in the backupServer
    backupServer->_getChildren(id, children, 10, childCount);
    if (childCount == 0) {
        return -1;
    }

    // loop registering all the children
    for (; childCount > 0; childCount--) {
        locateName(children[childCount - 1], name, 1024);
        registerType(id, name, 1, tmpID);
    }

    return 0;
}

/*
 * Retrieve all the children of a given id... limited to return given amount
 */
SysStatus
TypeMgr::getChildren(const TypeID id, uval64 *children,
                     uval arraySize, uval &outSize)
{
    uval i = 0;
    void *ptr;
    TypeMgrEntry *e, *child;

    // locate the entry to update
    e = locate(id);

    // loop through the children
    ptr = e->childList.next(NULL, child);
    while (ptr && i < arraySize) {
        children[i] = child->id;
        i++;

        ptr = e->childList.next(ptr, child);
    }
    outSize = i;

    // if we didn't get them all, make a note of that
    // FIXME: if this is happening, we need to make this more generic
    if (ptr != NULL) {
        return -1;
    }
    return 0;
}
