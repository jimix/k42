/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVSemaphores.C,v 1.12 2005/05/11 00:11:23 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sys V Semaphores
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>

#include "SysVSemaphores.H"
#include <sync/atomic.h>
#include "meta/MetaSysVSemaphores.H"
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sync/Sem.H>

// Must define a number of structures here for internal use
struct sem {
    BaseSemaphore<BLock> val; /* the actual semaphore */
    int pid;                  /* pid of last operation */
};

#define SEMMNI  1024            /* <= IPCMNI  max # of semaphore identifiers */
#define SEMMSL  250             /* <= 8 000 max num of semaphores per id */
#define SEMMNS  (SEMMNI*SEMMSL) /* <= INT_MAX max # of semaphores in system */
#define SEMOPM  32	        /* <= 1 000 max num of ops per semop call */
#define SEMVMX  32767           /* <= 32767 semaphore maximum value */
#define SEMAEM  SEMVMX          /* adjust on exit max value */

/* unused */
#define SEMUME  SEMOPM          /* max num of undo entries per process */
#define SEMMNU  SEMMNS          /* num of undo structures system wide */
#define SEMMAP  SEMMNS          /* # of entries in semaphore map */
#define SEMUSZ  20		/* sizeof struct sem_undo */



// destructor for SemInfo
SysVSemaphores::SemInfo::~SemInfo() {
    freeGlobal(base, sizeof(struct sem) * stat.sem_nsems);
}

/* static */ SysVSemaphores *SysVSemaphores::obj;

/* static */ void
SysVSemaphores::ClassInit()
{
    MetaSysVSemaphores::init();
    obj = new SysVSemaphores();
    obj->init();
}


void
SysVSemaphores::init(void)
{
    semID = 1;

    info.semmap = SEMMAP;
    info.semmni = SEMMNI;
    info.semmns = SEMMNS;
    info.semmnu = SEMMNU;
    info.semmsl = SEMMSL;
    info.semopm = SEMOPM;
    info.semume = SEMUME;
    info.semusz = SEMUSZ;
    info.semvmx = SEMVMX;
    info.semaem = SEMAEM;

    usedSems = 0;

}

SysStatusUval
SysVSemaphores::makeSemaphore(sval32 key, uval32 num, uval flags)
{
    SemInfo *si;
    uval sz;
    uval perm;

    if (num == 0) {
	return _SERROR(2054, 0, EINVAL);
    }

    if ((int)(usedSems + num) > info.semmns) {
	return _SERROR(2054, 0, ENOSPC);
    }

    si = new SemInfo;
    if (si == NULL) {
	return _SERROR(2449, 0, ENOMEM);
    }

    // allocate list of semaphores
    sz = num * sizeof(struct sem);
    si->base = (struct sem *)allocGlobal(sz);
    if (si->base == NULL) {
	delete si;
	return _SERROR(2450, 0, ENOMEM);
    }
    memset(si->base, 0, sz);

    for (unsigned int i = 0; i < num; i++) {
        si->base[i].val.init(0);
    }

    // initialize lock
    si->lock.init();

    // FIXME: need some sort of reuse policy!!
    si->semID = FetchAndAddSynced(&(obj->semID), 1);
    AtomicAdd32Synced(&(obj->usedSems), num);

    perm = (flags & (S_IRWXU|S_IRWXG|S_IRWXO));

    si->stat.sem_perm.__key	= key;
    si->stat.sem_perm.uid	= 0;	// uid of owner
    si->stat.sem_perm.gid	= 0;	// gid of owner
    si->stat.sem_perm.cuid	= 0;	// uid of creator
    si->stat.sem_perm.cgid	= 0;	// gid of creator
    si->stat.sem_perm.mode	= perm;	// perms
    si->stat.sem_perm.__seq	= si->semID; // FIXME: I THINK


    si->stat.sem_otime		= 0; // must be zero
    si->stat.sem_ctime		= 0; // now
    si->stat.sem_nsems		= num;

    si->pending = NULL;
    si->pending_last = (struct sem_queue *)&si->pending;
    //si->undo = NULL;

    AutoLock<BLock> al(&obj->lock);
    if (key != IPC_PRIVATE)
	obj->semaphoreByKey.add(key, si);
    obj->semaphoreBySemId.add(si->semID, si);

    return _SRETUVAL(si->semID);
}

/* static */ SysStatusUval
SysVSemaphores::SemGet(sval32 key, uval32 num, uval flags)
{
    SemInfo *si = NULL;

    if (IPC_PRIVATE == key ) {
	// create the semaphore anew (we think this is what IPC_PRIVATE
	// means.  This semaphore is _not_ hashed by key
	return obj->makeSemaphore(key, num, flags);
    } else {
	// get this logic out of the way
	obj->lock.acquire();
	obj->semaphoreByKey.find(key, si);
	obj->lock.release();

	if (si == NULL) {
	    // Semaphore does not exist
	    if (!(flags & IPC_CREAT)) {
	    // We are NOT creating
	    return _SERROR(2452, 0, ENOENT);
	    }

	    // Create the Semaphore
	    return obj->makeSemaphore(key, num, flags);
	}

	if (flags & IPC_EXCL) {
            // require exclusive region
            return _SERROR(2453, 0, EEXIST);
	}  	
	return _SRETUVAL(si->semID);
    }
}

/* static */ SysStatusUval
SysVSemaphores::SemOperation(sval32 id, struct sembuf *ops, uval nops,
                             __CALLER_PID pid)
{
    SysStatus rc = 0;
    struct sem *semP;
    SemInfo *si = NULL;

    // FIXME: should support more than one operation at a time
    if (nops > 1) {
	return _SERROR(2522, 0, E2BIG);
    }

    // locate the proper semaphore set
    obj->lock.acquire();

    obj->semaphoreBySemId.find(id, si);
    if (si == NULL) {
        obj->lock.release();
        return _SERROR(2523, 0, EIDRM);
    }

    // lock the semaphore set
    si->lock.acquire();
    obj->lock.release();

    // check that semaphore number is valid
    if (ops->sem_num >= si->stat.sem_nsems) {
        si->lock.release();
        return _SERROR(2524, 0, EFBIG);
    }

    // grab the semaphore and perform the proper operation
    semP = &si->base[ops->sem_num];

    if (ops->sem_op > 0) {
        semP->val.V(ops->sem_op);

    } else if (ops->sem_op == 0) {
        if (ops->sem_flg & IPC_NOWAIT) {
            if (!semP->val.tryZ()) {
                si->lock.release();
                return _SERROR(2525, 0, EAGAIN);
            }
        } else {
            rc = semP->val.Z(&si->lock);
        }

    } else if (ops->sem_op < 0) {
        int count = -(ops->sem_op);

        if (ops->sem_flg & IPC_NOWAIT) {
            if (!semP->val.tryP(count)) {
                si->lock.release();
                return _SERROR(2526, 0, EAGAIN);
            }
        } else {
            rc = semP->val.P(&si->lock, count);
        }
    }

    // if there was an error while waiting, the lock was not re-acquired
    if (rc != 0) {
        return _SERROR(2527, 0, rc);
    }

    // update the pid
    semP->pid = pid;

    si->lock.release();
    return _SRETUVAL(rc);
}

/* static */ SysStatus
SysVSemaphores::SemControlIPC(uval32 id, uval32 cmd, struct semid_ds *buf)
{
    SemInfo *si = NULL;
    SysStatus rc = 0;
    AutoLock<BLock> al(&obj->lock);

    if (!obj->semaphoreBySemId.find(id, si)) {
	return _SERROR(2454, 0, EIDRM);
    }

    switch (cmd) {
    case IPC_STAT:
        buf->sem_nsems = si->stat.sem_nsems;
	// memcpy(buf, &si->stat, sizeof(struct semid_ds));
	break;
    case IPC_SET:
        si->stat.sem_perm.uid = buf->sem_perm.uid;
        si->stat.sem_perm.gid = buf->sem_perm.gid;
        si->stat.sem_perm.mode = buf->sem_perm.mode & 0x1FF;
	break;
    default:
        err_printf("SysVSemaphores: %u requested in SemControlIPC\n", cmd);
	rc = _SERROR(2056, 0, EINVAL);
	break;
    }

    return rc;
}

/* static */ SysStatus
SysVSemaphores::SemControlRMID(uval32 id)
{
    SemInfo *si = NULL;
    SysStatus rc = 0;
    AutoLock<BLock> al(&obj->lock);

    if (!obj->semaphoreBySemId.find(id, si)) {
	return _SERROR(2455, 0, EIDRM);
    }

    // remove from the hash tables
    obj->semaphoreBySemId.remove(id, si);
    if (si->stat.sem_perm.__key != IPC_PRIVATE)
	obj->semaphoreByKey.remove(si->stat.sem_perm.__key, si);

    // send an error to all waiting threads
    for (unsigned int i = 0; i < si->stat.sem_nsems; i++) {
        si->base[i].val.wakeup(EIDRM);
    }

    // destroy the set and return
    delete si;
    return rc;
}

/* static */ SysStatus
SysVSemaphores::SemControlInfo(uval32 id, uval32 cmd, struct seminfo *buf)
{
    SysStatus rc = 0;

    switch (cmd) {
    case IPC_INFO:
    case SEM_STAT:
    case SEM_INFO:
	memcpy(buf, &obj->info, sizeof(struct seminfo));
	break;
    default:
	rc = _SERROR(2057, 0, EINVAL);
	break;
    }
    return rc;
}

/* static */ SysStatusUval
SysVSemaphores::SemControlSetVal(uval32 id, uval32 num, uval32 val)
{
    SemInfo *si = NULL;
    AutoLock<BLock> al(&obj->lock);

    if (!obj->semaphoreBySemId.find(id, si)) {
	return _SERROR(2456, 0, EIDRM);
    }

    if (num >= si->stat.sem_nsems) {
        return _SERROR(2482, 0, EINVAL);
    }

    if ((int)val > obj->info.semvmx) {
        return _SERROR(2483, 0, ERANGE);
    }

    // no need to lock the semaphore set because global lock holds
    si->base[num].val.set(val);

    return _SRETUVAL(0);
}

/* static */ SysStatusUval
SysVSemaphores::SemControlGetVal(uval32 id, uval32 cmd, uval32 num)
{
    SemInfo *si = NULL;
    AutoLock<BLock> al(&obj->lock);
    sval zCnt, nCnt;
    uval rc = (uval)-1;

    if (!obj->semaphoreBySemId.find(id, si)) {
	return _SERROR(2528, 0, EIDRM);
    }

    if (num >= si->stat.sem_nsems) {
        return _SERROR(2529, 0, EINVAL);
    }

    // no need to lock the semaphore set because global lock holds
    switch (cmd) {
    case GETVAL:
        rc = si->base[num].val.get();
        break;
    case GETPID:
        rc = si->base[num].pid;
        break;
    case GETNCNT:
        si->base[num].val.waitCount(zCnt, nCnt);
        rc = nCnt;
        break;
    case GETZCNT:
        si->base[num].val.waitCount(zCnt, nCnt);
        rc = zCnt;
        break;
    }

    return _SRETUVAL(rc);
}

/* static */ SysStatusUval
SysVSemaphores::SemControlArray(uval32 id, uval32 cmd,
                                uval &cnt, unsigned short *array)
{
    uval16 i;
    SemInfo *si = NULL;
    AutoLock<BLock> al(&obj->lock);

    if (!obj->semaphoreBySemId.find(id, si)) {
        tassert(0, err_printf("SysVSemaphores: couldn't find id %d\n", id));
	return _SERROR(2530, 0, EIDRM);
    }

    tassert(cnt == si->stat.sem_nsems,
            err_printf("SysVSemaphores: set size mismatch\n"));

    switch (cmd) {
    case GETALL:
        for (i = 0; i < cnt; i++) {
            array[i] = si->base[i].val.get();
        }
        break;

    case SETALL:
        for (i = 0; i < cnt; i++) {
            if ((int)array[i] > obj->info.semvmx) {
                return _SERROR(2207, 0, ERANGE);
            }
        }

        err_printf("SetAll: ");
        for (i = 0; i < cnt; i++) {
            err_printf("%u ", array[i]);
            si->base[i].val.set(array[i]);
        }
        err_printf("\n");
        break;
    default:
        return _SRETUVAL(-1);
    }

    return _SRETUVAL(0);
}

