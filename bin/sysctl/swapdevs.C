/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Control active swap devices
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "FileTemplate.H"
#include <misc/ListSimpleLocked.H>
#include <io/DiskClient.H>
#include <stdlib.h>
#include <stub/StubDiskSwap.H>

class VirtSwapListFile:public VirtNamedFile{
protected:
    SysStatus insert_locked(char* name, void* spot) {
	SysStatus rc;
	SwapEntry *se = new SwapEntry;
	rc = DiskClient::Create(se->dcr, name);
	if (_FAILURE(rc)) {
	    err_printf("DiskClient::Create failed: %lx\n",rc);
	    delete se;
	    return rc;
	}

	ObjectHandle tmp;
	DREF(se->dcr)->getOH(tmp);
	StubObj stub(StubBaseObj::UNINITIALIZED);
	stub.setOH(tmp);

	rc = stub._giveAccess(se->oh, _KERNEL_PID,
			      MetaObj::controlAccess|
			      MetaObj::read|MetaObj::write,
			      MetaObj::none, StubBlockDev::typeID());


	if (_FAILURE(rc)) {
	    err_printf("_giveAccess to disk failed: %lx\n",rc);
	    DREF(se->dcr)->destroy();
	    delete se;
	    return rc;
	}

	StubDiskSwap sds(StubBaseObj::UNINITIALIZED);
	rc = sds._attachDevice(se->oh,DREF(se->dcr)->getSize());
	if (_FAILURE(rc)) {
	    err_printf("attachDevice failed: %lx\n",rc);
	    DREF(se->dcr)->destroy();
	    delete se;
	    return rc;
	}

	err_printf("Added swap device: %s\n",name);


	se->name = (char*)allocGlobal(strlen(name)+1);
	memcpy(se->name, name, strlen(name)+1);

	swapDevList.insertNext(spot, se);
	return 0;
    }
public:
    struct SwapEntry{
	DEFINE_GLOBAL_NEW(SwapEntry);
	char* name;
	DiskClientRef dcr;
	ObjectHandle oh;
    };
    uval length;
    ListSimpleLocked<SwapEntry*,AllocGlobal> swapDevList;

    DEFINE_GLOBAL_NEW(VirtSwapListFile);
    VirtSwapListFile(char *fname) {
	init((mode_t)0600);
	name = fname;
    }

    virtual SysStatus deleteFile() {
	return 0;
    }
    virtual SysStatus getServerFileType() {
	return VirtFSInfo;
    }
    virtual SysStatus _getMaxReadSize(uval &max, uval token=0) {
	max = 3072;
	return 0;
    }
    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff (char *buf, uval length, uval offset,
				    uval userData, uval token=0) {
	char* tmp = (char*)alloca(length+offset+1024);
	uval len = _read(tmp, length+offset, userData);
	if (len<offset) {
	    return 0;
	}
	len -= offset;
	if (length<len) {
	    len = length;
	}
	memcpy(buf, tmp+offset, len);
	return len;
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength,
				 uval userData, uval token=0)
    {
	char* curr = buf;
	swapDevList.acquireLock();
	SwapEntry *se;
	for (void* place = swapDevList.next(NULL,se);
	    place!=NULL;
	    place = swapDevList.next(NULL,se)) {
	    uval l = strlen(se->name);
	    if (l > buflength) {
		l = buflength;
	    }
	    strncpy(curr, se->name, l);
	    curr+=  l;
	    buflength -= l;
	    if (buflength) {
		curr[0]='\n';
		--buflength;
		++curr;
	    }
	    if (!buflength) {
		break;
	    }
	}
	swapDevList.releaseLock();
	return (uval)(curr-buf);
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __in uval userData, uval token=0) {
	SysStatus rc;
	uval written = 0;
	char *tmp=(char*)alloca(length+1);
	memcpy(tmp,buf,length);
	tmp[length]=0;
	swapDevList.acquireLock();
	while (1) {
	    char *next = strchr(tmp,'\n');
	    if (next) {
		next[0]=0;
		++next;
	    }
	    SwapEntry *present;

	    //Check the list to see if the name being added is already there
	    void *place = swapDevList.next(NULL,present);
	    void *prev = NULL;
	    do {
		sval cmp;

		//In the middle of the list, compare names to see if
		//we should insert, or if at end of list, append
		if (place) {
		    cmp = strcmp(tmp, present->name);

		    if (cmp>0 && swapDevList.next(place,present)==NULL) {
			//Force an append to the end of the list
			prev=place;
			cmp = -1;
		    }
		} else {
		    //Empty list, insert unconditionally
		    cmp = -1;
		}

		if (cmp<0) {
		    //appending to end of list, need to move prev up by one
		    if (cmp>0) {
			prev = place;
		    }
		    rc = insert_locked(tmp, prev);
		    if (_FAILURE(rc)) {
			swapDevList.releaseLock();
			if (written) {
			    return written;
			}
			return rc;
		    }
		    break;
		}

		if (cmp == 0) {
		    swapDevList.releaseLock();
		    if (written) {
			return written;
		    }
		    return _SERROR(2278, 0, EALREADY);
		}

		prev = place;
		place = swapDevList.next(prev, present);
	    } while (place);
	    written += strlen(tmp)+1;
	}
	swapDevList.releaseLock();
	return written;
    }
    virtual SysStatus _open(uval oflags, uval userData, uval &token) {
	return 0;
    }
    virtual SysStatus _close(uval userData, uval token=0) {
	// nothing to do on close
	return 0;
    }
};


void swapdevs_init(FileInfoVirtFSDir* sysFS) {
    addFile(new VirtSwapListFile("swapdevs"), sysFS);
}
