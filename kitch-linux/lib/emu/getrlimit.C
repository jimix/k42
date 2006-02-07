/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getrlimit.C,v 1.9 2004/12/16 23:09:03 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: the getrlimit call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/resource.h>

/* copied from linux arch/ppc64/kernel/sys_ppc32.c note they linux has
 * not automatic strategy for keeping this consistent with struct
 * rusage etc.
*/
struct timeval32
{
	int tv_sec, tv_usec;
};

struct rusage32 {
        struct timeval32 ru_utime;
        struct timeval32 ru_stime;
        int    ru_maxrss;
        int    ru_ixrss;
        int    ru_idrss;
        int    ru_isrss;
        int    ru_minflt;
        int    ru_majflt;
        int    ru_nswap;
        int    ru_inblock;
        int    ru_oublock;
        int    ru_msgsnd; 
        int    ru_msgrcv; 
        int    ru_nsignals;
        int    ru_nvcsw;
        int    ru_nivcsw;
};

void put_rusage (struct rusage32 *ru, struct rusage *r)
{
#define put_user(from, to) to = from;
	put_user (r->ru_utime.tv_sec, ru->ru_utime.tv_sec);
	put_user (r->ru_utime.tv_usec, ru->ru_utime.tv_usec);
	put_user (r->ru_stime.tv_sec, ru->ru_stime.tv_sec);
	put_user (r->ru_stime.tv_usec, ru->ru_stime.tv_usec);
	put_user (r->ru_maxrss, ru->ru_maxrss);
	put_user (r->ru_ixrss, ru->ru_ixrss);
	put_user (r->ru_idrss, ru->ru_idrss);
	put_user (r->ru_isrss, ru->ru_isrss);
	put_user (r->ru_minflt, ru->ru_minflt);
	put_user (r->ru_majflt, ru->ru_majflt);
	put_user (r->ru_nswap, ru->ru_nswap);
	put_user (r->ru_inblock, ru->ru_inblock);
	put_user (r->ru_oublock, ru->ru_oublock);
	put_user (r->ru_msgsnd, ru->ru_msgsnd);
	put_user (r->ru_msgrcv, ru->ru_msgrcv);
	put_user (r->ru_nsignals, ru->ru_nsignals);
	put_user (r->ru_nvcsw, ru->ru_nvcsw);
	put_user (r->ru_nivcsw, ru->ru_nivcsw);
#undef put_user
}

#ifdef __cplusplus
extern "C" {
#endif

int
__k42_linux_ugetrlimit (int resource, struct rlimit *rlim)
{
#undef getrlimit
    rlim->rlim_cur = RLIM_INFINITY;
    rlim->rlim_max = RLIM_INFINITY;
    return 0;
}

#define COMPAT_RLIM_INFINITY		0xffffffff
typedef uval32 compat_ulong_t;

struct compat_rlimit {
	compat_ulong_t	rlim_cur;
	compat_ulong_t	rlim_max;
};



long
__k42_linux_compat_sys_getrlimit
    (unsigned int resource, struct compat_rlimit *rlim)
{
    struct rlimit r;
    int ret;
    ret = __k42_linux_ugetrlimit(resource, &r);
    if(!ret) {
	if (r.rlim_cur > COMPAT_RLIM_INFINITY)
	    r.rlim_cur = COMPAT_RLIM_INFINITY;
	if (r.rlim_max > COMPAT_RLIM_INFINITY)
	    r.rlim_max = COMPAT_RLIM_INFINITY;
	rlim->rlim_cur = r.rlim_cur;
	rlim->rlim_max = r.rlim_max;
    }
    return ret;
}

int
__k42_linux_getrusage
    (sval who, struct rusage *usage)
{
    usage->ru_utime.tv_sec = 0;
    usage->ru_utime.tv_usec = 0;
    usage->ru_stime.tv_sec = 0;
    usage->ru_stime.tv_usec = 0;
    usage->ru_maxrss = 0;
    usage->ru_ixrss = 0;
    usage->ru_idrss = 0;
    usage->ru_isrss = 0;
    usage->ru_minflt = 0;
    usage->ru_majflt = 0;
    usage->ru_nswap = 0;
    usage->ru_inblock = 0;
    usage->ru_oublock = 0;
    usage->ru_msgsnd = 0; 
    usage->ru_msgrcv = 0; 
    usage->ru_nsignals = 0;
    usage->ru_nvcsw = 0;
    usage->ru_nivcsw = 0;

    SysStatus rc;
    BaseProcess::ResourceUsage resourceUsage;

    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getResourceUsage(0, resourceUsage);
    SYSCALL_EXIT();

    if (_FAILURE(rc)) {
	return -EINVAL;
    }

    usage->ru_minflt = resourceUsage.minflt;
    usage->ru_majflt = resourceUsage.majflt;

    return 0;
}
    
int
__k42_linux_getrusage_32
    (sval who, struct rusage32 *usage32)
{
    SysStatus rc;
    struct rusage usage;
    rc = __k42_linux_getrusage(who, &usage);
    put_rusage(usage32, &usage);
    return 0;
}
    
    
#ifdef __cplusplus
}
#endif

