#ifdef __KFS_INCS_H_
#error <kfsIncs.H> should be included exactly once from every .C file.
#else
#define __KFS_INCS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kfsIncs.H,v 1.23 2004/03/07 00:45:54 lbsoares Exp $
 *****************************************************************************/

// includes for tools and non-tools code
#include "defines.H"

// defines endian translation
#include "Byteorder.H"

#ifdef KFS_TOOLS

// if we are in the tools, fake out the kernel code
#include <sys/types.H>

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#define ALIGN_UP(addr, align)   (addr + ((align)-1) & ~((align)-1))

#define DEFINE_GLOBAL_NEW(x)
#define DEFINE_NOOP_NEW(x)
#define DEFINE_ALLOC_NEW(x)
#define DEFINE_GLOBALPADDED_NEW(x)

// FIXME: this has to be done in a better way!
#define _SERROR(x, y, z) (-z)
#define _SGENCD(rc) (-rc)

typedef sval SysStatus;
typedef sval SysStatusUval;

extern void KFSfree(void *ptr);
extern void* KFSalloc(uval size);

class KFSAlloc {
public:
    static void *alloc(uval size) {
	return KFSalloc(size);
    }
    static void  free(void *ptr, uval size) {
	KFSfree(ptr);
    }
};

class AllocGlobal : public KFSAlloc {};
class AllocLocalStrict : public KFSAlloc {};
class AllocGlobalPadded : public KFSAlloc {};
class AllocPinnedLocalStrict : public KFSAlloc {};
class AllocPinnedGlobal : public KFSAlloc {};
class AllocPinnedGlobalPadded : public KFSAlloc {};

#define __BLOCK_H_
class BLock {
    uval dummy;
public:
    void init() { dummy = 0; }
    void acquire() { dummy = 1; }
    uval tryAcquire() { return 1; }
    void release() { dummy = 0; }
    uval isLocked() { return dummy; }
};

// following copied from <sync/Lock.H>
template <class Lock>
class AutoLock {
    Lock *lock;
public:
    // Beware: the constructor using a reference **does not get inlined**
    AutoLock(Lock *l) : lock(l)         { lock->acquire(); }
    AutoLock(Lock &l) : lock(&l)        { lock->acquire(); }

    DEFINE_NOOP_NEW(AutoLock);
    ~AutoLock()                         { lock->release(); }
};

#define _ASSERT_HELD(x)

#define __RW_BLOCK_H_
class RWBLock {
public:
    void init() {}
    void acquireR() {}
    void releaseR() {}
    void acquireW() {}
    void releaseW() {}
};

#define LOG_PAGE_SIZE 12

#define __SYS_INCS_H_
#define __KFS_DISK_H_

#define err_printf(strargs...) printf(strargs)
#define _SUCCESS(rc) ((rc) >= 0)
#define _FAILURE(rc)  ((rc) < 0)
#define _IF_FAILURE_RET(ss)  if ((ss)<0) return (ss);
#define tassert(x, y)          do { if (!(x)) {y;} assert(x);} while (0)
#define tassertMsg(x, args...) do { if (!(x)) {printf(args);} assert(x);} while (0)
#define tassertWrn(x, args...) do { if (!(x)) {printf(args);}} while (0)
#define passert(x, y)          do { if (!(x)) {y;} assert(x);} while (0)
#define passertMsg(x, args...) do { if (!(x)) {printf(args);} assert(x);} while (0)

#define MS_RDONLY 1

class FSFile {
public:
    // kludge internal to file system to enable asserts on type
    enum { FSFILE_NONE, FSFILE_GLOBAL, FSFILE_KFS, FSFILE_NFS,
	   FSFILE_K42RAMFS };
    struct FreeServerFileNode {
    };
};

class FileLinux {
public:
    struct Stat : stat {
    };
};

class ServerFile;
class DirLinuxFS;
class MultiLinkMgrLock;

typedef ServerFile    **ServerFileRef;
typedef DirLinuxFS    **DirLinuxFSRef;
typedef char            PathName;
typedef uval            ProcessID;
typedef uval            ObjectHandle;
typedef uval            TypeID;        // Unique TypeID of a Service/Obj Class

class MultiLinkManager {
public:
    class SFHolder {
    };
    static SFHolder* AllocHolder(ServerFileRef fref) { return NULL; }
    void add(uval, SFHolder *) {}
};

#else

// otherwise, just include the system headers
#include <sys/sysIncs.H>
#include <sync/BLock.H>

#endif

#endif
