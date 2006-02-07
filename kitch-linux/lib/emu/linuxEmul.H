/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxEmul.H,v 1.64 2005/08/31 13:55:59 dgurley Exp $
 ****************************************************************************/
/*****************************************************************************
 * Module Description: Misc header stuff
 * **************************************************************************/

#ifndef __LINUX_EMUL_H_
#define __LINUX_EMUL_H_

#include <sys/types.h>
#include <sys/errno.h>
#include <scheduler/Scheduler.H>
#include <trace/traceUser.h>
#include <sys/ProcessLinuxClient.H>
#include <misc/linkage.H>
//Get linux syscall numbers
#include <asm/unistd.h>

#define SIGN_EXT(x) uval(sval32(x))

// make sure we set glibc's errno
#define __set_errno(val) { Scheduler::GetCurThreadPtr()->errno = (val); }

#ifndef strong_alias
# define strong_alias(name, aliasname) _strong_alias(name, aliasname)
# define _strong_alias(name, aliasname) \
  extern "C" __typeof (name) aliasname __attribute__ ((alias (#name)));
#endif /* #ifndef strong_alias */

#ifndef weak_alias
# define weak_alias(name, aliasname) _weak_alias(name, aliasname)
# define _weak_alias(name, aliasname) \
  extern "C" __typeof (name) aliasname __attribute__ ((alias (#name),weak));
#endif /* #ifndef strong_alias */

#define LINUX_TRACE_SYSCALL
#ifdef LINUX_TRACE_SYSCALL
#define LINUX_TRACE_ENTER()
#define LINUX_TRACE_EXIT() 

#else
#define LINUX_TRACE_ENTER()
#define LINUX_TRACE_EXIT()
#endif


#define SYSCALL_ENTER()         { LINUX_TRACE_ENTER(); }

#define SYSCALL_EXIT()          { LINUX_TRACE_EXIT(); }

#define SYSCALL_DEACTIVATE()      ProcessLinuxClient::SyscallDeactivate()
#define SYSCALL_ACTIVATE()        ProcessLinuxClient::SyscallActivate()
#define SYSCALL_BLOCK()           ProcessLinuxClient::SyscallBlock()
#define SYSCALL_SIGNALS_PENDING() ProcessLinuxClient::SyscallSignalsPending()

// provide extern "C" dcls for interfaces
#include <scheduler/Scheduler.H>
// as you move things from linuxEmul.C and implement them
// put them here as well.
#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

// extern "C" these K42 introduced interfaces
extern pid_t __k42_linux_parentPID;
extern signed long __k42_linux_emulNoSupport (const char *, signed long);
extern void LinuxFileInit(uval iofmSize, uval iofmBufPtr);
extern void __k42_linux_fileFini (void);
extern int __k42_linux_spawn (const char *, char *const [], char *const [], int);


/*
 * Each file will uniquely name its funtion by prefixing it with
 * __k42_linux_.  That is, function foo() becomes __k42_linux_foo(),
 * before the linux headers. This way the linux headers will extern "C"
 * the interface for us and we will then maximize prototype checking.
 * .
 * Unfortunately there are a few corner cases where the linux headers
 * play their own games with the prototypes, usually when glibc introduces a
 * translation layer.
 * If this is the case then they must be declared in this extern "C" block.
 *
 * DISCLAIMER:
 *    If this disgusts you (and it should) please remember that this
 *    whole __k42_linux_ stuff will go away once we push this code into glibc.
 */

extern int __k42_linux_open (const char *, int, mode_t);
extern int __k42_linux_mkdir (const char *, mode_t);
extern int __k42_linux_statfs (const char *, struct statfs *);
extern int __k42_linux_xstat (int, const char *, struct stat *);
extern int __k42_linux_fxstat (int, int, struct stat *);
extern int __k42_linux_lxstat(int, const char *, struct stat *);
extern int __k42_linux_chmod (const char *, mode_t);
extern int __k42_linux_fchmod (int, mode_t);
extern int __k42_linux_fchdir (int);
extern int __k42_linux_fchown (int fd, uid_t owner, gid_t group);
extern int __k42_linux_ftruncate (int, off_t);
extern int __k42_linux_mknod(const char *pathname, uval, uval);
extern int __k42_linux_dup (int);
extern int __k42_linux_dup2 (int, int);
extern int __k42_linux_fcntl (int, int, ...);
extern long __k42_linux__llseek (int,unsigned long,unsigned long,loff_t*,int);
extern off_t __k42_linux_lseek (int, off_t, int);
extern int __k42_linux_personality(unsigned long);
extern int __k42_linux_pipe (int []);

extern mode_t __k42_linux_umask (mode_t mask);

extern int __k42_linux__newselect (int n, fd_set *readfds,
                                   fd_set *writefds, fd_set *exceptfds,
                                   struct timeval *timeout);
extern int __k42_linux_ipc (uint call, int first, int second, long third,
			 void *ptr, long fifth);
#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

//This is a flag to determine if this process is eligible for "fast exec"
extern uval useLongExec;

// from linux net/compat.h
/*
 * A pointer passed in from user mode. This should not
 * be used for syscall parameters, just declare them
 * as pointers because the syscall entry code will have
 * appropriately comverted them already.
 */
typedef uval32          compat_uptr_t;

static inline void *compat_ptr(compat_uptr_t uptr)
{
	return (void *)(unsigned long)uptr;
}

#endif /* #ifndef __LINUX_EMUL_H_ */
