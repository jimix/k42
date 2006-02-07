/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DentryList.C,v 1.20 2005/04/21 04:35:01 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "DentryList.H"
#include <misc/StringTable.I>
#include <stub/StubSystemMisc.H>

DentryListLinear::DentryListLinear()
{
    NameHolder tmpNameHolder;
    tmpNameHolder._obj    = 0;
    tmpNameHolder.strLen  = 1;
    tmpNameHolder.str[0]  = '.';
    tmpNameHolder.setDirSF(0);
    tmpNameHolder.rwlock = (NameHolderInfo::LockType*) AllocGlobal::alloc
	(sizeof(NameHolderInfo::LockType));
    tmpNameHolder.rwlock->init();

    table.init(&tmpNameHolder);
}

/* virtual */ void
DentryListLinear::lookupPtr(char *name, uval namelen,
			    NameHolderInfo* &nameHolder)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DentryListLinear::lookupPtr\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    STE<NameHolder> *steNameHolder = table.getHead();
    int numEntries = table.getNumEntries();

    int i=0;
    // FIXME: should realy do a lookup function in StrTable
    while (i++ < numEntries) {
	if (namelen == (uval)steNameHolder->getContents()->strLen) {
	    if (memcmp(name, steNameHolder->getContents()->str, namelen)==0) {
		nameHolder = steNameHolder->getContents();
		return;
	    }
	}
	steNameHolder = steNameHolder->getNext();
    }

    nameHolder = NULL;
}

/* virtual */ SysStatus
DentryListLinear::remove(char *name, uval namelen, 
			 NameHolderInfo *nhi /* = NULL*/)
{
    NameHolderInfo *nh;
    lookupPtr(name, namelen, nh);
    if (nh == NULL) {
	return _SERROR(2176, 0, ENOENT);
    } else {
	if (nhi) {
	    nh->assignTo(nhi);
	}
	if (nh->isSymLinkFSFile()) {
	    // Incrementing the published mount version number here will
	    // invalid all MountPointMgr client's caches
	    SysStatusUval rc = StubSystemMisc::_IncrementMountVersionNumber();
	    tassertMsg(_SUCCESS(rc), "what happenned? rc=%ld\n", rc);
	    
	    if (nh->symlink) {
		AllocGlobal::free(nh->symlink, nh->symlink->symlinkLength +
				  sizeof(NameHolderInfo::SymlinkInfo));
	    }
	}
	return table.deleteEntryWithPtr((NameHolder*)nh);
    }
}

/* virtual */ void
DentryListLinear::lookupObj(ObjRef ref, NameHolderInfo* &nhi)
{
    STE<NameHolder> *steNameHolder = table.getHead();
    int numEntries = table.getNumEntries();
    NameHolder *nh;
    int i=0;
    // FIXME: should realy do a lookup function in StrTable
    while (i++ < numEntries) {
	if (steNameHolder->getContents()->_obj == ref) {
	    nh = steNameHolder->getContents();
	    nhi = nh;
	    return;
	}
	steNameHolder = steNameHolder->getNext();
    }

    nh = NULL;
}

/* virtual */ SysStatus
DentryList::lookup(ObjRef fref, NameHolderInfo *nhi)
{
    NameHolderInfo *nh;
    lookupObj(fref, nh);
    if (nh == NULL) {
	return _SERROR(2415, 0, ENOENT);
    }
    tassertMsg(nh->isFSFile() == 0, "?"); // only used for file/dir entries
    nh->assignTo(nhi);
    return 0;
}

/* virtual */ SysStatus
DentryList::lookup(ObjRef fref, char* &name, uval &len)
{
    NameHolderInfo *nh;
    lookupObj(fref, nh);
    if (nh == NULL) {
	return _SERROR(2416, 0, ENOENT);
    }
    getName(nh, name, len);
    tassertMsg(nh->isFSFile() == 0, "?");
    return 0;
}

/* virtual */ SysStatus
DentryListLinear::remove(ObjRef ref, NameHolderInfo *nhi /* = NULL */)
{
    NameHolderInfo *nh;
    lookupObj(ref, nh);
    if (nh == NULL) {
	return _SERROR(2422, 0, ENOENT);
    }
    if (nhi != NULL) {
	nh->assignTo(nhi);
    }
    if (nh->isSymLinkFSFile()) {
	// Incrementing the published mount version number here will
	// invalid all MountPointMgr client's caches
	SysStatusUval rc = StubSystemMisc::_IncrementMountVersionNumber();
	tassertMsg(_SUCCESS(rc), "what happenned? rc=%ld\n", rc);
	
	if (nh->symlink) {
	    AllocGlobal::free(nh->symlink, nh->symlink->symlinkLength +
			      sizeof(NameHolderInfo::SymlinkInfo));
	}
    }
    return table.deleteEntryWithPtr((NameHolder*)nh);
}

/* virtual */ SysStatus
DentryList::lookup(char *name, uval namelen, NameHolderInfo *nhi)
{
    NameHolderInfo *nh;
    lookupPtr(name, namelen, nh);
    if (nh == NULL) {
	return _SERROR(2498, 0, ENOENT);
    } else {
	nh->assignTo(nhi);
	return 0;
    }
}

/* virtual */ SysStatus
DentryList::lookup(char *name, uval namelen, void* &retEntry)
{
    NameHolderInfo *nh;
    lookupPtr(name, namelen, nh);
    if (nh == NULL) {
	return _SERROR(2394, 0, ENOENT);
    } else {
	retEntry = (void*) nh;
	return 0;
    }
}

/* virtual */ SysStatus
DentryListLinear::updateOrAdd(char *name, uval len, void *anEntry,
			      NameHolderInfo *nhi)
{
    if (anEntry == NULL) {
	NameHolder *nh = table.allocEntry(len);
	tassert((nh!=0), err_printf("FIXME:alloc failed\n"));
	memcpy(nh->str, name, len);
	nh->strLen  = len;
	nh->rwlock = (NameHolderInfo::LockType*) AllocGlobal::alloc
	    (sizeof(NameHolderInfo::LockType));
	nh->rwlock->init();
	nhi->rwlock = nh->rwlock;
	nh->assignFrom(nhi);
    } else {
	NameHolder *inNH = (NameHolder*) anEntry;
	nhi->rwlock = inNH->rwlock;
	inNH->assignFrom(nhi);
    }
    return 0;
}

/* virtual */ void*
DentryListLinear::getNext(void *curr, NameHolderInfo *nhi)
{
    uval index = (uval) curr;
    if (index >= table.getNumEntries()) {
	return NULL;
    }

    STE<NameHolder> *steNameHolder = table.getHead();
    uval j = 0;
    while (j++ < index) {
	steNameHolder = steNameHolder->getNext();
    }
    NameHolder *nh = steNameHolder->getContents();
    nh->assignTo(nhi);
    return (void*) (index+1);
}

DentryListHash::DentryListHash()
{
    numEntries = 0;
    destroyInvoked = 0;
}

/* virtual */ void
DentryListHash::lookupPtr(char *name, uval len, NameHolderInfo* &entry)
{
    uval key = GetKey(name, len);
    BucketList *bl;
    uval ret = hashTable.find(key, bl);
    if (ret) {
	void *curr = NULL;
	HashEntry *e;
	while ((curr = bl->next(curr, e)) != NULL) {
	    if (e->strLen == len && memcmp(e->str, name, len) == 0) {
		// found
		entry = e;
		return;
	    }
	}
    }

    entry = NULL;
}

/* virtual */ SysStatus
DentryListHash::remove(char *name, uval len, NameHolderInfo *nhi /* = NULL */)
{
    uval key = GetKey(name, len);
    NameHolderInfo *entry;
    lookupPtr(name, len, entry);
    if (entry == NULL) {
	return _SERROR(2423, 0, ENOENT);
    } else {
	if (nhi) {
	    entry->assignTo(nhi);
	}
	if (entry->isSymLinkFSFile()) {
	    // Incrementing the published mount version number here will
	    // invalid all MountPointMgr client's caches
	    SysStatusUval rc = StubSystemMisc::_IncrementMountVersionNumber();
	    tassertMsg(_SUCCESS(rc), "what happenned? rc=%ld\n", rc);

	    // free any cached data
	    if (entry->symlink) {
		AllocGlobal::free(entry->symlink, entry->symlink->symlinkLength +
				  sizeof(NameHolderInfo::SymlinkInfo));
	    }
	}
	numEntries--;
	BucketList *bl;
	uval ret = hashTable.find(key, bl);
	tassertMsg(ret == 1, "?");
	bl->remove((HashEntry*)entry);
	delete (HashEntry*)entry;
	// FIXME: we could throw away the bucket if we wanted ...
    }
    return 0;
}

// Notice that this is costly, but it's only used when dealing with
// DirLinuxFSVolatile objects that are disappearing (e.g. stale file in
// NFS server
/* virtual */ void
DentryListHash::lookupObj(ObjRef ref, NameHolderInfo* &entry)
{
    uval key;
    BucketList *bl;
    uval ret = hashTable.getFirst(key, bl);
    while (ret) {
	void *curr = NULL;
	HashEntry *e;
	while ((curr = bl->next(curr, e)) != NULL) {
	    if (e->_obj == ref) { // found
		entry = e;
		return;
	    }
	}
	ret = hashTable.getNext(key, bl);
    }
    entry = NULL;
}

// Notice that this is costly, but it's only used when dealing with
// DirLinuxFSVolatile objects that are disappearing (e.g. stale file in
// NFS server
/* virtual */ SysStatus
DentryListHash::remove(ObjRef ref, NameHolderInfo *nhi)
{
    uval key;
    BucketList *bl;
    uval ret = hashTable.getFirst(key, bl);
    while (ret) {
	void *curr = NULL;
	HashEntry *e;
	while ((curr = bl->next(curr, e)) != NULL) {
	    if (e->_obj == ref) { // found
		if (e->isSymLinkFSFile()) {
		    // Incrementing the published mount version number here will
		    // invalid all MountPointMgr client's caches
		    SysStatusUval rc;
		    rc = StubSystemMisc::_IncrementMountVersionNumber();
		    tassertMsg(_SUCCESS(rc), "what happenned? rc=%ld\n", rc);

		    // free any cached data
		    if (e->symlink) {
			AllocGlobal::free(e->symlink, 
					  e->symlink->symlinkLength + 
					  sizeof(NameHolderInfo::SymlinkInfo));
		    }
		}
		uval r = bl->remove(e);
		tassertMsg(r == 1, "?");
		numEntries--;
		return 0;
	    }
	}
	ret = hashTable.getNext(key, bl);
    }

    return _SERROR(2424, 0, ENOENT);
}

/* virtual */ SysStatus
DentryListHash::updateOrAdd(char *name, uval len, void *anEntry, 
			    NameHolderInfo *nhi)
{
    if (anEntry == NULL) {
	NameHolderInfo *e;
	lookupPtr(name, len, e);

	if (!e) {
	    // Entry was not there, create it and add.
	    uval key = GetKey(name, len);
	    HashEntry *nh = new HashEntry(name, len);
	    tassert((nh!=0), err_printf("FIXME:alloc failed\n"));
	    nh->rwlock = (NameHolderInfo::LockType*) AllocGlobal::alloc
		(sizeof(NameHolderInfo::LockType));
	    nh->rwlock->init();
	    nhi->rwlock = nh->rwlock;
	    nh->assignFrom(nhi);
	    addHash(key, nh);
	} else {
	    // just update the current entry
	    e->assignFrom(nhi);
	}
	numEntries++;
    } else {
	HashEntry *inNH = (HashEntry*) anEntry;
	nhi->rwlock = inNH->rwlock;
	inNH->assignFrom(nhi);
    }
    return 0;
}

/* virtual */ void*
DentryListHash::getNext(void *curr, NameHolderInfo *nhi)
{
    uval index = (uval) curr;
    if (index >= numEntries) {
	return NULL;
    }
    uval j = 0;
    uval key;
    BucketList *bl;
    uval ret = hashTable.getFirst(key, bl);
    while (ret) {
	void *cr = NULL;
	HashEntry *e;
	while ((cr = bl->next(cr, e)) != NULL) {
	    if (j == index) {
		e->assignTo(nhi);
		return (void*)(index+1);
	    }
	    j++;
	    tassertMsg(j < numEntries, "?");
	}
	ret = hashTable.getNext(key, bl);
    }
    tassertMsg(0, "should not reach here\n");
    return NULL;
}

/* virtual */ void*
DentryListHash::getNext(void *curr, HashEntry **nhi)
{
    uval index = (uval) curr;
    if (index >= numEntries) {
	return NULL;
    }
    uval j = 0;
    uval key;
    BucketList *bl;
    uval ret = hashTable.getFirst(key, bl);
    while (ret) {
	void *cr = NULL;
	HashEntry *e;
	while ((cr = bl->next(cr, e)) != NULL) {
	    if (j == index) {
		*nhi = e;
		return (void*)(index+1);
	    }
	    j++;
	    tassertMsg(j < numEntries, "?");
	}
	ret = hashTable.getNext(key, bl);
    }
    tassertMsg(0, "should not reach here\n");
    return NULL;
}

void
DentryListHash::addHash(uval key, HashEntry *e)
{
    BucketList* bl;
    uval ret = hashTable.find(key, bl);
    if (ret == 1) {
	// use list we already have
	bl->add(e);
    } else {
	bl = new BucketList;
	bl->add(e);
	hashTable.add(key, bl);
    }
}

/* virtual */
DentryListHash::~DentryListHash()
{
    tassertMsg(destroyInvoked == 1, "destroy not invoked?");
}

/* virtual */ SysStatus
DentryListHash::destroy()
{
    tassertMsg(destroyInvoked == 0, "?");

    // FIXME: For now we're only deleting this type of hash in situations
    // they are empty already, but this is not the right thing to do in
    // general
    tassertMsg(numEntries == 0, "table not empty\n");
    uval key;
    BucketList *bl;
    uval restart = 0;
    while(hashTable.removeNext(key, bl, restart)) {
	void *curr = NULL;
	HashEntry *e;
	curr = bl->next(curr, e);
	tassertMsg(curr == NULL, "has to be empty\n");
	delete bl;
    }

    hashTable.destroy();

    destroyInvoked = 1;

    return 0;
}

