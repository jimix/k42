#ifndef __FILE_SYSTEM_LIST__
#define __FILE_SYSTEM_LIST__
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemList.H,v 1.2 2004/01/05 14:54:27 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <misc/ListSimpleLocked.H>

class FileSystemList {
private:
    struct FSInstance {
	ObjRef obj;
	char path[PATH_MAX+1];
	DEFINE_GLOBAL_NEW(FSInstance);
	FSInstance(ObjRef o, char *p) : obj(o) {
	    strncpy(path, p, strlen(p));
	}
    };
    ListSimpleLocked<FSInstance*,AllocGlobal> list;
public:
    DEFINE_GLOBAL_NEW(FileSystemList);
    FileSystemList() {}
    ~FileSystemList() {
	FSInstance *fsi;
	while (list.removeHead(fsi)) {
	    delete fsi;
	}
    }

    // returns NULL if not found
    ObjRef find(char *mpath, uval len) {
	void *curr = NULL;
	FSInstance *fsi;
	list.acquireLock();
	while ((curr = list.next(curr, fsi))) {
	    tassertMsg(fsi != NULL, "?");
	    if (strncmp(fsi->path, mpath, len) == 0) {
		list.releaseLock();
		return fsi->obj;
	    }
	}
	list.releaseLock();
	return NULL;
    }
    void add(ObjRef o, char *p) {
	FSInstance *fsi = new FSInstance(o, p);
	list.add(fsi);
    }
    // return NULL if there is no next element, if curr is NULL, returns head
    void *next(void *curr, ObjRef &o) {
	FSInstance *fsi;
	void *ret = list.next(curr, fsi);
	if (ret) {
	    o = fsi->obj;
	}
	return ret;
    }
    uval isEmpty() { return list.isEmpty(); }
};    
#endif // #ifndef __FILE_SYSTEM_LIST__
