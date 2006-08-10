#ifndef __DENTRY_LIST_H
#define __DENTRY_LIST_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DentryList.H,v 1.13 2005/04/21 04:35:01 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <misc/StringTable.H>
#include <scheduler/Scheduler.H> // needed by RWLock.H ?!
#include <sync/RWLock.H>
#include "fs_defines.H"
#include <misc/HashSimple.H>
#include <misc/ListSimple.H>

class DirLinuxFS;
class ServerFile;
class FSFile;
typedef DirLinuxFS      **DirLinuxFSRef;
typedef ServerFile      **ServerFileRef;

class NameHolderInfo {
public:
    /* The intention of the lock for this nameHolder is to guarantee that
     * the object won't be destroyed as we have invocations going on, without
     * locking the whole directory for the duration of the operation on this
     * particular entry. The idea is to find this entry, lock the nameHolder,
     * release the lock on the directory, use it, and then unlock the 
     * nameHolder. Between finding the nameHolder and unlocking it,
     * we could have a number of additions to the children list, and
     * the current implementation 
     * of this list (DynamicStrTable of NameHolders) may change the location
     * in memory of the nameHolder (therefore making the reference to the
     * lock to be released once the operation is over obsolete).
     * For now we'll avoid this problem by storing the address of the lock
     * in the dynamic data structure, instead of the lock itself.
     * Not clear if keeping this dynamic table is a good choice in the long
     * run. */
    typedef RWBLock LockType;
    // this needs to be a pointer and not an embedded object here, until we
    // change a lot of code in fslib
    LockType  *rwlock;
    union {
	DirLinuxFSRef  dref;
	ServerFileRef  fref;
	FSFile         *fsFile;
	ObjRef         _obj;
    };

    /*
     * if only FSFILE is set, not converted, 
     * if DIR is set, then it is a server object representing dir
     * if !FSFILE && !DIR then it is a server object representing a file
     * if FSFILE && SYMLINK then it is a fsFile representing a symlingk
     * if FSFILE && OTHER then it is a fsFile representing a remote object
     *   like a pipe or socket
     * Finds out that it is a symlink or other lazily, i.e., don't care if it is
     * until there is an attempt to actually open the file, so is just a 
     * generic fsFile until then. 
     */ 
    static const uval IS_DIR    = 1;
    static const uval IS_FSFILE = 2;
    static const uval IS_SYMLNK = 4;
    static const uval IS_OTHER  = 8;
    uval8 flags; // determines what type of file/object this is


    struct SymlinkInfo {
	uval16 symlinkLength;
	char  symlinkValue[0];
    };
    SymlinkInfo *symlink;

    uval8 getFlags() {return flags;}
    void  setFlags(uval8 fl) {flags = fl;}

    uval isConverted() {
	// if only IS_FSFILE is set, then not converted
	if((flags & IS_FSFILE) && ((flags & IS_FSFILE) == flags)) { return 0; }
	return 1;
    }

    uval isFSFile() { return !!(flags & IS_FSFILE); }

    uval isDirSF() { return !!(flags & IS_DIR); }
    void setDirSF(DirLinuxFSRef d) { 
	flags = IS_DIR;
	dref = d;
	symlink = NULL;
    }

    void setFileSF(ServerFileRef r) { 
	flags = 0;
	fref = r;
	symlink = NULL;
    }

    void setFileFSFile(FSFile *f) { 
	flags = IS_FSFILE;
	fsFile = f;
	symlink = NULL;
    }

    uval isSymLinkFSFile() { return !!(flags & IS_SYMLNK); }
    void setSymLink(FSFile *f) {
	flags = IS_SYMLNK | IS_FSFILE;
	fsFile = f;
	symlink = NULL;
    }	

    uval isOtherFSFile() { return !!(flags & IS_OTHER); }
    void setOther(FSFile *f) {
	flags = IS_OTHER | IS_FSFILE;
	fsFile = f;
	symlink = NULL;
    }
    
    void assignTo(NameHolderInfo *nhi) {
	nhi->rwlock = rwlock;
	nhi->_obj = _obj;
	nhi->flags = flags;
	nhi->symlink = symlink;
    }

    void assignFrom(NameHolderInfo *nhi) {
	rwlock   = nhi->rwlock;
	_obj     = nhi->_obj;
	flags    = nhi->flags;
	symlink  = nhi->symlink;
    }
};

class DentryList {
protected:
    virtual void lookupPtr(char *name, uval namelen,
			   NameHolderInfo* &nameHolder)=0;
    virtual void lookupObj(ObjRef ref, NameHolderInfo* &nh)=0;
    virtual void getName(NameHolderInfo *nh, char* &name, uval &len)=0;

public:
    
    virtual SysStatus lookup(char *name, uval namelen, NameHolderInfo *nhi);
    virtual SysStatus lookup(char *name, uval namelen, void* &retEntry);
    virtual SysStatus lookup(ObjRef fref, NameHolderInfo *nhi);

    virtual SysStatus lookup(ObjRef, char* &name, uval &len);

    virtual uval isEmpty()=0;
    virtual void* getNext(void *curr, NameHolderInfo *nhi)=0;
    virtual SysStatus remove(char *name, uval namelen,
			     NameHolderInfo *nhi = NULL)=0;
    virtual SysStatus remove(ObjRef fref, NameHolderInfo *nhi = NULL)=0;
    virtual SysStatus updateOrAdd(char *name, uval len,
				  void *anEntry, NameHolderInfo *nhi)=0;
    virtual SysStatus destroy()=0;

    DEFINE_NOOP_NEW(DentryList);
    virtual ~DentryList() { }
};

// FIXME: DentryListLinear is leaking rwlock. Leaving it for now, since
// we're not using this implementation for the current paper crunch.
// (We need to make rwlock embedded and all this will sort out nicely!)
class DentryListLinear : public DentryList {
    class NameHolder : public NameHolderInfo {
    public:
	// these two are required for a string table entry
	uval8 strLen;			// length of string
	char  str[1];			// place holder for string
    };
    
    _DynamicStrTable<NameHolder,AllocGlobal> table;

protected:
    virtual void lookupPtr(char *name, uval namelen,
			   NameHolderInfo* &nameHolder);
    virtual void lookupObj(ObjRef ref, NameHolderInfo* &nh);
    virtual void getName(NameHolderInfo *nhi, char* &name, uval &len) {
	name = ((NameHolder*)nhi)->str;
	len = ((NameHolder*)nhi)->strLen;
    }

public:
    DEFINE_GLOBAL_NEW(DentryListLinear);
    DentryListLinear();

    virtual ~DentryListLinear() {
	// FIXME: clean up!
	tassertMsg(0, "If you're using DentryListLinear, be aware that it"
		   " the descructor has not been implemented!\n");
    }
    virtual SysStatus destroy() {
	// FIXME: clean up!
	tassertMsg(0, "If you're using DentryListLinear, be aware that it"
		   " the destructor has not been implemented!\n");
	return 0;
    }

    virtual uval isEmpty() {
	return table.getNumEntries() == 1;
    }
    virtual SysStatus remove(char *name, uval namelen,
			     NameHolderInfo *nhi = NULL);
    virtual SysStatus remove(ObjRef fref, NameHolderInfo *nhi = NULL);
    virtual SysStatus updateOrAdd(char *name, uval len, void *anEntry,
				  NameHolderInfo *nhi);
    virtual void* getNext(void *curr, NameHolderInfo *nhi);
};

class DentryListHash : public DentryList {
public:  // making it public because FileSystemK42RamFS is using this
    class HashEntry : public NameHolderInfo {
    public:
	// these two are required for a string table entry
	uval32 strLen;			// length of string
	char  *str;
	DEFINE_GLOBAL_NEW(HashEntry);
	HashEntry() {}
	HashEntry(char *name, uval len) {
	    strLen = len;
	    str = (char*) AllocGlobalPadded::alloc(len);
	    memcpy(str, name, len);
	}
	~HashEntry() {
	    AllocGlobal::free(rwlock, sizeof(NameHolderInfo::LockType));
	    AllocGlobalPadded::free(str, strLen);
	}
    };
    typedef ListSimple<HashEntry*, AllocGlobal> BucketList;
    HashSimple<uval, BucketList*, AllocGlobal, 0> hashTable;
    uval numEntries;

    // From Linux
    static const uval INITNAMEHASH = 0;
    static uval PartialNameHash(uval c, uval prevHash) {
	return (prevHash + (c << 4) + (c >> 4)) * 11;
    }
    static uval FullNameHash(const char *name, uval len) {
	uval hash = INITNAMEHASH;
	while (len--)
		hash = PartialNameHash(*name++, hash);
	return hash;
    }
    static uval GetKey(char *name, uval len) {
	return FullNameHash(name, len);
    }
    uval destroyInvoked;

protected:
    virtual void lookupPtr(char *name, uval namelen,
			   NameHolderInfo* &nameHolder);
    virtual void lookupObj(ObjRef ref, NameHolderInfo* &nh);
    virtual void getName(NameHolderInfo *nhi, char* &name, uval &len) {
	name = ((HashEntry*)nhi)->str;
	len = ((HashEntry*)nhi)->strLen;
    }
    void addHash(uval key, HashEntry *e);

public:
    DEFINE_GLOBAL_NEW(DentryListHash);
    DentryListHash();
    virtual ~DentryListHash();
    virtual SysStatus destroy();

    virtual uval isEmpty() {
	return numEntries == 0;
    }
    virtual SysStatus remove(char *name, uval namelen,
			     NameHolderInfo *nhi = NULL);
    virtual SysStatus remove(ObjRef fref, NameHolderInfo *nhi = NULL);
    virtual SysStatus updateOrAdd(char *name, uval len, void *anEntry,
				  NameHolderInfo *nhi);
    virtual void* getNext(void *curr, NameHolderInfo *nhi);

    // only for this subclass; not available in DentryList itfc
    uval getNumEntries() {return numEntries;}
    virtual void* getNext(void *curr, HashEntry **nhi);
};



#endif // #ifndef __DENTRY_LIST_H
