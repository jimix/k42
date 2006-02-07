#ifndef __SysVSharedMem_H_
#define __SysVSharedMem_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVSharedMem.H,v 1.3 2003/03/14 23:14:17 marc Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Sys V Shared Memory
 ****************************************************************************/

#include <misc/HashSimple.H>

#ifndef EXPORT_SYSVSHAREDMEM
#define EXPORT_SYSVSHAREDMEM
#include <sys/shm.h>
#endif // #ifndef EXPORT_SYSVSHAREDMEM

class SysVSharedMem : private Obj {
    BLock lock;
    static SysVSharedMem *obj;
    uval shmID;

    struct RegionInfo {
	ObjectHandle frOH;
	uval addr;
	sval32 shmID;
	struct shmid_ds shmInfo;
	DEFINE_GLOBALPADDED_NEW(RegionInfo);
    };

#ifndef _IN_STUBGEN
    HashSimple<sval, RegionInfo *, AllocGlobal, 0> regionsByKey;
    HashSimple<sval, RegionInfo *, AllocGlobal, 0> regionsByShmID;
    HashSimple<uval, RegionInfo *, AllocGlobal, 0> regionsByAddr;
#endif
    static SysStatusUval MakeRegion(sval32 key, uval size, uval flags);

    DEFINE_GLOBALPADDED_NEW(SysVSharedMem);
public:

    SysVSharedMem() {;}
    ~SysVSharedMem() {;}

    static void ClassInit();

__xprotected:
    static SysStatusUval ShmGet(__in sval32 key,
				__in uval size,
				__in uval flags);

    static SysStatusUval ShmAttach(__in sval32 id,
				   __in uval flags,
				   __out ObjectHandle &frOH,
				   __CALLER_PID pid);

    static SysStatus ShmDetach(__in uval addr);
    static SysStatus ShmControl(__in uval32 id,
				__in uval32 cmd,
				__inout struct shmid_ds &buf);
};


#endif /* #ifndef __SysVSharedMem_H_ */
