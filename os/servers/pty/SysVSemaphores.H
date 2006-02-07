#ifndef __SysVSemaphores_H_
#define __SysVSemaphores_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVSemaphores.H,v 1.4 2004/10/01 18:34:44 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Sys V Shared Memory
 ****************************************************************************/

#include <misc/HashSimple.H>

#ifndef EXPORT_SYSVSEMAPHORES
#define EXPORT_SYSVSEMAPHORES
#include <sys/sem.h>
#endif // #ifndef EXPORT_SYSVSEMAPHORES

// the System V semaphore server object
class SysVSemaphores : private Obj {
    BLock lock;
    static SysVSemaphores *obj;
    uval semID;
    uval test;

    // This structure holds information for a single semaphore set
    struct SemInfo {
	BLock lock;
	sval32 semID;
	struct sem *base;

	struct semid_ds stat;
	struct sem_queue * pending;
	struct sem_queue * pending_last;
	//undo;  FIXME  do something about this fiels which was previously
	// contained in the semid_ds struct.  Currently no code uses the
	// field.
	DEFINE_GLOBALPADDED_NEW(SemInfo);

#ifndef _IN_STUBGEN
        ~SemInfo();
#endif
    };

    struct seminfo info;
    uval32 usedSems;

#ifndef _IN_STUBGEN
    HashSimple<sval32, SemInfo *, AllocGlobal, 0> semaphoreByKey;
    HashSimple<sval32, SemInfo *, AllocGlobal, 0> semaphoreBySemId;
#endif

    void init();
    SysStatusUval makeSemaphore(sval32 key, uval32 num, uval flags);

    DEFINE_GLOBALPADDED_NEW(SysVSemaphores);
public:

    SysVSemaphores() {;}
    ~SysVSemaphores() {;}

    static void ClassInit();

__xprotected:
    static SysStatusUval SemGet(__in sval32 key,
				__in uval32 num,
				__in uval flags);

    static SysStatusUval SemOperation(__in sval32 id,
				      __inbuf(nops) struct sembuf *ops,
				      __in uval nops,
                                      __CALLER_PID pid);

    static SysStatus SemControlIPC(__in uval32 id,
				   __in uval32 cmd,
				   __inout struct semid_ds *buf);

    static SysStatus SemControlRMID(__in uval32 id);

    static SysStatus SemControlInfo(__in uval32 id,
				    __in uval32 cmd,
				    __out struct seminfo *buf);

    static SysStatusUval SemControlSetVal(__in uval32 id,
					  __in uval32 num,
					  __in uval32 val);

    static SysStatusUval SemControlGetVal(__in uval32 id,
					  __in uval32 cmd,
					  __in uval32 num);

    static SysStatus SemControlArray(__in uval32 id,
				     __in uval32 cmd,
                                     __inout uval &cnt,
                                     __inoutbuf(cnt:cnt:cnt) uval16 *array);
};

#endif /* #ifndef __SysVSemaphores_H_ */
