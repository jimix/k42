/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kill.C,v 1.28 2005/04/18 19:57:11 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: the various get calls
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/ProcessLinux.H>

#undef PROFILE_SIGSUSPEND
#ifdef PROFILE_SIGSUSPEND
#include <trace/traceLock.h>
#include <misc/linkage.H>
#endif // PROFILE_SIGSUSPEND

#ifdef __cplusplus
extern "C" {
#endif

int
__k42_linux_kill (pid_t pid, int sig)
{
#undef kill
    SysStatus rc;
    SYSCALL_ENTER();

    if (sig == SIGABRT) {
	pid_t mypid;
	DREFGOBJ(TheProcessLinuxRef)->getpid(mypid);
	if (pid == mypid) {
	    // Probably an abort() call:
	    BREAKPOINT;
	}
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->kill(pid, sig);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return 0;
}

// taken from Linux 2.6.26 ppc64 signal.h
typedef void (*__sighandler_t)(int);
struct lk_sigaction {
	void (*lk_sa_handler)(int);
	unsigned long lk_sa_flags;
	void (*lk_sa_restorer)(void);
	sigset_t lk_sa_mask;		/* mask last for extensibility */
};

int
__k42_linux_sigaction (int signum, const struct lk_sigaction* act,
		       struct lk_sigaction* oldact)
{
    SysStatus rc;
    struct sigaction new_sa, old_sa;
    SYSCALL_ENTER();

    // FIXME: we assume a sigset is 64 bits

    if (act != NULL) {
	new_sa.sa_handler = act->lk_sa_handler;
	new_sa.sa_flags = act->lk_sa_flags;
	new_sa.sa_restorer = act->lk_sa_restorer;
	new_sa.sa_mask.__val[0] = act->lk_sa_mask.__val[0];
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->
			    sigaction(signum,
				      (act != NULL) ? &new_sa : NULL,
				      (oldact != NULL) ? &old_sa : NULL,
				      sizeof(uval64));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    if (oldact != NULL) {
	oldact->lk_sa_handler = old_sa.sa_handler;
	oldact->lk_sa_flags = old_sa.sa_flags;
	oldact->lk_sa_restorer = old_sa.sa_restorer;
	oldact->lk_sa_mask.__val[0] = old_sa.sa_mask.__val[0];
    }

    SYSCALL_EXIT();
    return 0;
}

int
__k42_linux_rt_sigaction (int signum, const struct lk_sigaction* act,
		          struct lk_sigaction* oldact, size_t sigsetsize)
{
    SysStatus rc;
    struct sigaction new_sa, old_sa;
    SYSCALL_ENTER();

    uval words = ((sigsetsize - 1) / sizeof(new_sa.sa_mask.__val[0])) + 1;

    if (act != NULL) {
	new_sa.sa_handler = act->lk_sa_handler;
	new_sa.sa_flags = act->lk_sa_flags;
	new_sa.sa_restorer = act->lk_sa_restorer;
	for (uval i = 0; i < words; i++) {
	    new_sa.sa_mask.__val[i] = act->lk_sa_mask.__val[i];
	}
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->
			    sigaction(signum,
				      (act != NULL) ? &new_sa : NULL,
				      (oldact != NULL) ? &old_sa : NULL,
				      sigsetsize);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    if (oldact != NULL) {
	oldact->lk_sa_handler = old_sa.sa_handler;
	oldact->lk_sa_flags = old_sa.sa_flags;
	oldact->lk_sa_restorer = old_sa.sa_restorer;
	for (uval i = 0; i < words; i++) {
	    oldact->lk_sa_mask.__val[i] = old_sa.sa_mask.__val[i];
	}
    }

    SYSCALL_EXIT();
    return 0;
}

#if 0
int
__k42_linux_sigaltstack (const struct sigaltstack* ss, struct sigaltstack* oss)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
}


int
__k42_linux_sigpending (sigset_t* set)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
}
#endif


int
__k42_linux_sigprocmask (int how, const sigset_t* set, sigset_t* oldset)
{
    SysStatus rc;
    SYSCALL_ENTER();
    // FIXME: we assume a sigset is 64 bits
    rc = DREFGOBJ(TheProcessLinuxRef)->sigprocmask(how, set, oldset,
						   sizeof(uval64));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}

int
__k42_linux_rt_sigprocmask (int how, const sigset_t* set, sigset_t* oldset,
			    size_t sigsetsize)
{
    SysStatus rc;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->sigprocmask(how, set, oldset,
						   sigsetsize);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}

int
__k42_linux_sigsuspend (const sigset_t* mask)
{
    SysStatus rc;
    SYSCALL_ENTER();

#ifdef PROFILE_SIGSUSPEND
    SysTime start = 0;	// initializer needed to avoid compilation warning
    uval callChain[3];
    if (traceLockEnabled()) {
	start = Scheduler::SysTimeNow();
	GetCallChainSelf(6, callChain, 3);
    }
#endif // PROFILE_SIGSUSPEND

    rc = DREFGOBJ(TheProcessLinuxRef)->sigsuspend(mask);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    do {
	SYSCALL_BLOCK();
    } while (!SYSCALL_SIGNALS_PENDING());

#ifdef PROFILE_SIGSUSPEND
    TraceOSLockContendBlock(/*lock*/ 0, /*spinCount*/ 0,
	       (Scheduler::SysTimeNow() - start),
	       callChain[0], callChain[1], callChain[2],
	       0, 0);
#endif // PROFILE_SIGSUSPEND

    SYSCALL_EXIT();
    return 0;
}

int
__k42_linux_rt_sigsuspend (const sigset_t* mask)
{
    return __k42_linux_sigsuspend(mask);
}

#if 0
int
__k42_linux_rt_sigpending (sigset_t* set, size_t sigsetsize)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
}


int
__k42_linux_rt_sigqueueinfo (int pid, int sig, siginfo_t *uinfo)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
}


int
__k42_linux_rt_sigtimedwait (const sigset_t *uthese, siginfo_t *uinfo,
			     const struct timespec *uts, size_t sigsetsize)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
}
#endif

int
__k42_linux_pause (void)
{
    /*
     * sleeps until signal is received then always return -1
     */
    SYSCALL_ENTER();
    do {
	SYSCALL_BLOCK();
    } while (!SYSCALL_SIGNALS_PENDING());
    SYSCALL_EXIT();
    return -EINTR;
}

// use ppc64 versions - may need to make machine dependent
// from linux compat.h
#define _COMPAT_NSIG		64
#define _COMPAT_NSIG_BPW	32

typedef uval32		compat_sigset_word;

#define _COMPAT_NSIG_WORDS	(_COMPAT_NSIG / _COMPAT_NSIG_BPW)

typedef struct {
	compat_sigset_word	sig[_COMPAT_NSIG_WORDS];
} compat_sigset_t;



// from linux ppc32.h
struct sigaction32 {
/* Really a pointer, but need to deal with 32 bits */
    unsigned int  compat_sa_handler;	
    unsigned int sa_flags;
    unsigned int compat_sa_restorer;	/* Another 32 bit pointer */
    compat_sigset_t sa_mask;		/* A 32 bit mask */
};

/* FIXME: This function has not been tested yet.  When 32-bit signals are
 * working, come back and test it.
 */
int
__k42_linux_sys32_sigsuspend (const compat_sigset_t *mask)
{
    sigset_t mask64;

    // FIXME: we assume a sigset is 64 bits
    mask64.__val[0] = ((uval)(mask->sig[1])<<32) | mask->sig[0];

    return __k42_linux_sigsuspend(&mask64);
}

/* FIXME: This function has not been tested yet.  When 32-bit signals are
 * working, come back and test it.
 */
int
__k42_linux_sys32_sigaction (int signum, const struct sigaction32 *act,
			     struct sigaction32 *oldact)
{
    struct lk_sigaction new_ka, old_ka;
    int ret;

    if (act) {
	new_ka.lk_sa_handler = (void (*)(int)) (uval) act->compat_sa_handler;
	new_ka.lk_sa_restorer = (void (*)()) (uval) act->compat_sa_restorer;
	new_ka.lk_sa_flags = act->sa_flags;
	new_ka.lk_sa_mask.__val[0] = ((uval)(act->sa_mask.sig[1])<<32) |
	  act->sa_mask.sig[0];
    }

    ret = __k42_linux_sigaction(signum, act ? &new_ka : NULL,
				oldact ? &old_ka : NULL);

    if (!ret && oldact) {
	oldact->compat_sa_handler = (uval32)(uval)old_ka.lk_sa_handler;
	oldact->compat_sa_restorer = (uval32)(uval)old_ka.lk_sa_restorer;
	oldact->sa_flags = old_ka.lk_sa_flags;
	oldact->sa_mask.sig[0] = old_ka.lk_sa_mask.__val[0];
	oldact->sa_mask.sig[1] = old_ka.lk_sa_mask.__val[0]>>32;
    }
	
    return ret;
}

int
__k42_linux_sys32_rt_sigaction (int signum, const struct sigaction32 *act,
				struct sigaction32 *oldact, size_t sigsetsize)
{
    struct lk_sigaction new_ka, old_ka;
    int ret;

    /* XXX: Don't preclude handling different sized sigset_t's.  */
    if (sigsetsize != sizeof(compat_sigset_t))
	    return -EINVAL;

    // FIXME: we assume a sigset is 64 bits
    if (act) {
	new_ka.lk_sa_handler = (void (*)(int)) (uval) act->compat_sa_handler;
	new_ka.lk_sa_restorer = (void (*)()) (uval) act->compat_sa_restorer;
	new_ka.lk_sa_flags = act->sa_flags;
	new_ka.lk_sa_mask.__val[0] = ((uval)(act->sa_mask.sig[1])<<32) |
	    act->sa_mask.sig[0];
    }
    ret = __k42_linux_rt_sigaction(signum,
				   act ? &new_ka : NULL,
				   oldact ? &old_ka : NULL,
				   sigsetsize);
    if (!ret && oldact) {
	oldact->compat_sa_handler = (uval32)(uval)old_ka.lk_sa_handler;
	oldact->compat_sa_restorer = (uval32)(uval)old_ka.lk_sa_restorer;
	oldact->sa_flags = old_ka.lk_sa_flags;
	oldact->sa_mask.sig[0] = old_ka.lk_sa_mask.__val[0];
	oldact->sa_mask.sig[1] = old_ka.lk_sa_mask.__val[0]>>32;
    }
    return ret;
}

/* FIXME: This function has not been tested yet.  When 32-bit signals are
 * working, come back and test it.
 */
int
__k42_linux_sys32_rt_sigsuspend (const compat_sigset_t* mask)
{
    return __k42_linux_sys32_sigsuspend(mask);
}

/* FIXME: This function has not been tested yet.  When 32-bit signals are
 * working, come back and test it.
 */
int
__k42_linux_sys32_sigprocmask(int how,
			      const compat_sigset_t* set, 
			      compat_sigset_t* oldset)
{
    sigset_t new_set, old_set;
    int ret;

    // FIXME: we assume a sigset is 64 bits
    if (set) {
	new_set.__val[0] = ((uval)(set->sig[1])<<32) | set->sig[0];
    }

    ret = __k42_linux_sigprocmask(how, set ? &new_set : NULL,
				  oldset ? &old_set : NULL);

    if (!ret && oldset) {
	oldset->sig[0] = old_set.__val[0];
	oldset->sig[1] = old_set.__val[0]>>32;
    }

    return ret;
}

int
__k42_linux_sys32_rt_sigprocmask (int how, const  compat_sigset_t *set,
				  compat_sigset_t *oldset, size_t sigsetsize)
{
	sigset_t new_set, old_set;
	int ret;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(compat_sigset_t))
		return -EINVAL;

	// FIXME: we assume a sigset is 64 bits
	if (set) {
	    new_set.__val[0] = ((uval)(set->sig[1])<<32) |
		set->sig[0];
	}

	ret = __k42_linux_rt_sigprocmask
	    (how, set ? &new_set : NULL, oldset ? &old_set : NULL,
	     sizeof old_set);
	if (!ret && oldset) {
	    oldset->sig[0] = old_set.__val[0];
	    oldset->sig[1] = old_set.__val[0]>>32;
	}
	return ret;
}

int
__k42_linux_sigreturn(unsigned long /*__unused*/,
		      uval b, uval c, uval d, uval e, uval f, uval stkPtr)
{
    SYSCALL_ENTER();
    (void) DREFGOBJ(TheProcessLinuxRef)->
			    sigreturn(SignalUtils::SIGRETURN_64, stkPtr);
    SYSCALL_EXIT();
    return 0;	// won't really return to user at all
}

int
__k42_linux_rt_sigreturn(unsigned long /*__unused*/,
			 uval b, uval c, uval d, uval e, uval f, uval stkPtr)
{
    SYSCALL_ENTER();
    (void) DREFGOBJ(TheProcessLinuxRef)->
			    sigreturn(SignalUtils::SIGRETURN_RT_64, stkPtr);
    SYSCALL_EXIT();
    return 0;	// won't really return to user at all
}

int
__k42_linux_sigreturn_32(unsigned long /*__unused*/,
			 uval b, uval c, uval d, uval e, uval f, uval stkPtr)
{
    SYSCALL_ENTER();
    (void) DREFGOBJ(TheProcessLinuxRef)->
			    sigreturn(SignalUtils::SIGRETURN_32, stkPtr);
    SYSCALL_EXIT();
    return 0;	// won't really return to user at all
}

int
__k42_linux_rt_sigreturn_32(unsigned long /*__unused*/,
			    uval b, uval c, uval d, uval e, uval f, uval stkPtr)
{
    SYSCALL_ENTER();
    (void) DREFGOBJ(TheProcessLinuxRef)->
			    sigreturn(SignalUtils::SIGRETURN_RT_32, stkPtr);
    SYSCALL_EXIT();
    return 0;	// won't really return to user at all
}

#ifdef __cplusplus
}
#endif
