/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: socketcall.C,v 1.8 2005/05/03 21:18:28 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for ipc() syscall --- gate to real syscall impl.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

#define socket __k42_linux_socket
#define bind __k42_linux_bind
#define connect __k42_linux_connect
#define listen __k42_linux_listen
#define accept __k42_linux_accept
#define getsockname __k42_linux_getsockname
#define getpeername __k42_linux_getpeername
#define socketpair __k42_linux_socketpair
#define send __k42_linux_send
#define sendto __k42_linux_sendto
#define recv __k42_linux_recv
#define recvfrom __k42_linux_recvfrom
#define shutdown __k42_linux_shutdown
#define setsockopt __k42_linux_setsockopt
#define getsockopt __k42_linux_getsockopt
#define sendmsg __k42_linux_sendmsg
#define recvmsg __k42_linux_recvmsg

#include "stub/StubSysVSemaphores.H"
#include <sys/types.h>
#include <sys/socket.h>
typedef uval32 u32;
typedef sval32 s32;
#include <linux/socket.h>
#include <linux/net.h>

#undef VERBOSE_SOCKETCALL

/*
 *      System call vectors.
 *
 *      Argument checking cleaned up. Saved 20% in size.
 *  This function doesn't need to set the kernel lock because
 *  it is set by the callees.
 */

extern "C" long __k42_linux_socketcall(int call, unsigned long *args);
extern uval TraceSyscalls;


static inline void
pre_trace(const char *name, unsigned long *a)
{
    if (!TraceSyscalls) return;
    err_printf("\n\t\t%s 0x%lx 0x%lx 0x%lx 0x%lx", name, a[0], a[1], a[2], a[3]);
}

static inline void
pre_trace32(const char *name, uval32 *a)
{
    if (!TraceSyscalls) return;
    err_printf("\n\t\t%s 0x%x 0x%x 0x%x 0x%x", name, a[0], a[1], a[2], a[3]);
}

static inline void
post_trace(uval err)
{
    if (!TraceSyscalls) return;
    err_printf(" ret:0x%lx\n", err);
}

long __k42_linux_socketcall(int call, unsigned long *a)
{
    unsigned long a0,a1;
    int err;

    if (call<1||call>SYS_RECVMSG)
	return -EINVAL;

    a0=a[0];
    a1=a[1];

    switch (call) {
    case SYS_SOCKET:
	pre_trace("__k42_linux_socket", a);
	err = __k42_linux_socket(a0,a1,a[2]);
	break;
    case SYS_BIND:
	pre_trace("__k42_linux_bind", a);
	err = __k42_linux_bind(a0,(struct sockaddr *)a1, a[2]);
	break;
    case SYS_CONNECT:
	pre_trace("__k42_linux_connect", a);
	err = __k42_linux_connect(a0, (struct sockaddr *)a1, a[2]);
	break;
    case SYS_LISTEN:
	pre_trace("__k42_linux_listen", a);
	err = __k42_linux_listen(a0,a1);
	break;
    case SYS_ACCEPT:
	pre_trace("__k42_linux_accept", a);
	err = __k42_linux_accept(a0,(struct sockaddr *)a1, (socklen_t *)a[2]);
	break;
    case SYS_GETSOCKNAME:
	pre_trace("__k42_linux_getsockname", a);
	err = __k42_linux_getsockname(a0,(struct sockaddr *)a1,
                                      (socklen_t *)a[2]);
	break;
    case SYS_GETPEERNAME:
	pre_trace("__k42_linux_getpeername", a);
	err = __k42_linux_getpeername(a0, (struct sockaddr *)a1,
                                      (socklen_t *)a[2]);
	break;
    case SYS_SOCKETPAIR:
	pre_trace("__k42_linux_socketpair", a);
	err = __k42_linux_socketpair(a0,a1, a[2], (int *)a[3]);
	break;
    case SYS_SEND:
	pre_trace("__k42_linux_send", a);
	err = __k42_linux_send(a0, (void *)a1, a[2], a[3]);
	break;
    case SYS_SENDTO:
	pre_trace("__k42_linux_sendto", a);
	err = __k42_linux_sendto(a0,(void *)a1, a[2], a[3],
				 (struct sockaddr *)a[4], a[5]);
	break;
    case SYS_RECV:
	pre_trace("__k42_linux_recv", a);
	err = __k42_linux_recv(a0, (void *)a1, a[2], a[3]);
	break;
    case SYS_RECVFROM:
	pre_trace("__k42_linux_recvfrom", a);
	err = __k42_linux_recvfrom(a0, (void *)a1, a[2], a[3],
				   (struct sockaddr *)a[4], (socklen_t *)a[5]);
	break;
    case SYS_SHUTDOWN:
	pre_trace("__k42_linux_shutdown", a);
	err = __k42_linux_shutdown(a0,a1);
	break;
    case SYS_SETSOCKOPT:
	pre_trace("__k42_linux_setsockopt", a);
	err = __k42_linux_setsockopt(a0, a1, a[2], (void *)a[3], a[4]);
	break;
    case SYS_GETSOCKOPT: /* 0d15 */
	pre_trace("__k42_linux_getsockopt", a);
	err = __k42_linux_getsockopt(a0, a1, a[2], (void *)a[3],
				     (socklen_t *)a[4]);
	break;
    case SYS_SENDMSG:
	pre_trace("__k42_linux_sendmsg", a);
	err = __k42_linux_sendmsg(a0, (struct msghdr *) a1, a[2]);
	break;
    case SYS_RECVMSG:
	pre_trace("__k42_linux_recvmsg", a);
	err = __k42_linux_recvmsg(a0, (struct msghdr *) a1, a[2]);
	break;
    default:
	err = -EINVAL;
	break;
    }
    #ifdef VERBOSE_SOCKETCALL
    if (_FAILURE(err)) {
	err_printf("%s: call %d a0(fd)=%ld a1=%ld failed with %d\n",
		   __func__, call, a0, a1, -err);
    }
    #endif	// VERBOSE_SOCKETCALL
    post_trace(err);

    return err;
}

/*
 * from linux kernel 2.4.13
 */
#include <linux/net.h>

extern "C"
typedef	uval32		compat_uint_t;
typedef	sval32		compat_int_t;
typedef	uval32		compat_size_t;
struct compat_msghdr {
    compat_uptr_t   msg_name;       /* void * */
    compat_int_t    msg_namelen;
    compat_uptr_t   msg_iov;        /* struct compat_iovec * */
    compat_size_t   msg_iovlen;
    compat_uptr_t   msg_control;    /* void * */
    compat_size_t   msg_controllen;
    compat_uint_t   msg_flags;
};
struct compat_iovec  {
    compat_uptr_t   iov_base;
    compat_size_t   iov_len;
};
static void compat_msghdr(struct msghdr *msgh, struct compat_msghdr * cmsgh, struct iovec * iovecs)
{
	msgh->msg_name		= compat_ptr(cmsgh->msg_name);
	/* msg_namelen of msghdr is unsigned int */
	msgh->msg_namelen	= SIGN_EXT(cmsgh->msg_namelen);
        msgh->msg_iov		= iovecs;
	msgh->msg_iovlen	= cmsgh->msg_iovlen;
	msgh->msg_control	= compat_ptr(cmsgh->msg_control);
	msgh->msg_controllen	= cmsgh->msg_controllen;
	msgh->msg_flags		= cmsgh->msg_flags;
	msgh->msg_iov		= iovecs;
}
static void compat_iov(struct iovec * iovecs, struct compat_iovec * compat_iovecs, uint len)
{
	uval i;
	for (i = 0; i < len; i++)
	{
	    iovecs->iov_base	= compat_ptr(compat_iovecs->iov_base);
            iovecs->iov_len	= compat_iovecs->iov_len;
	    iovecs++;
	    compat_iovecs++;
	}
}

extern "C" long
__k42_linux_compat_sys_socketcall (int call, uval32 *a);

// from net/compat.c

long
__k42_linux_compat_sys_socketcall (int call, uval32 *a)
{
    int ret;
    uval32 a0, a1;

    if (call < SYS_SOCKET || call > SYS_RECVMSG)
	return -EINVAL;

    a0 = a[0];
    a1 = a[1];
//err_printf("%s: Call=%d  a0=%u  a1=%u\n", __PRETTY_FUNCTION__, call, a0, a1);

    switch (call) {
    case SYS_SOCKET:
	pre_trace32("__k42_linux_socket", a);
	ret = __k42_linux_socket(a0, a1, a[2]);
	break;
    case SYS_BIND:
	pre_trace32("__k42_linux_bind", a);
	ret = __k42_linux_bind(a0, (struct sockaddr *)compat_ptr(a1), a[2]);
	break;
    case SYS_CONNECT:
	pre_trace32("__k42_linux_connect", a);
	ret = __k42_linux_connect(a0, (struct sockaddr *)compat_ptr(a1), a[2]);
	break;
    case SYS_LISTEN:
	pre_trace32("__k42_linux_listen", a);
	ret = __k42_linux_listen(a0, a1);
	break;
    case SYS_ACCEPT:
	pre_trace32("__k42_linux_accept", a);
	ret = __k42_linux_accept(a0, (struct sockaddr *)compat_ptr(a1), (socklen_t *)compat_ptr(a[2]));
	break;
    case SYS_GETSOCKNAME:
	pre_trace32("__k42_linux_getsockname", a);
	ret = __k42_linux_getsockname(a0, (struct sockaddr *)compat_ptr(a1), (socklen_t *)compat_ptr(a[2]));
	break;
    case SYS_GETPEERNAME:
	pre_trace32("__k42_linux_getpeername", a);
	ret = __k42_linux_getpeername(a0, (struct sockaddr *)compat_ptr(a1), (socklen_t *)compat_ptr(a[2]));
	break;
    case SYS_SOCKETPAIR:
	pre_trace32("__k42_linux_socketpair", a);
	ret = __k42_linux_socketpair( a0, a1, a[2], (int*)(compat_ptr(a[3])));
	break;
    case SYS_SEND:
	pre_trace32("__k42_linux_send", a);
	ret = __k42_linux_send(a0, compat_ptr(a1), a[2], a[3]);
	break;
    case SYS_SENDTO:
	pre_trace32("__k42_linux_sendto", a);
	ret = __k42_linux_sendto(a0, compat_ptr(a1), a[2], a[3], (struct sockaddr *)compat_ptr(a[4]), a[5]);
	break;
    case SYS_RECV:
	pre_trace32("__k42_linux_recv", a);
	ret = __k42_linux_recv(a0, compat_ptr(a1), a[2], a[3]);
	break;
    case SYS_RECVFROM:
	pre_trace32("__k42_linux_recvfrom", a);
	ret = __k42_linux_recvfrom(a0, compat_ptr(a1), a[2], a[3], (struct sockaddr *)compat_ptr(a[4]), (socklen_t *)compat_ptr(a[5]));
	break;
    case SYS_SHUTDOWN:
	pre_trace32("__k42_linux_shutdown", a);
	ret = __k42_linux_shutdown(a0,a1);
	break;
#if 0
    case SYS_SETSOCKOPT:
	pre_trace32("__k42_linux_setsockopt", a);
	ret = compat_sys_setsockopt(a0, a1, a[2],
				compat_ptr(a[3]), a[4]);
	break;
    case SYS_GETSOCKOPT:
	pre_trace32("__k42_linux_getsockopt", a);
	ret = compat_sys_getsockopt(a0, a1, a[2],
				compat_ptr(a[3]), compat_ptr(a[4]));
	break;
#endif
    case SYS_SENDMSG:
	{
	pre_trace32("__k42_linux_sendmsg", a);
	struct compat_msghdr *cmsgh = (struct compat_msghdr *)compat_ptr(a1);
	struct compat_iovec *compat_iovecs = (struct compat_iovec *)compat_ptr(cmsgh->msg_iov);

	struct iovec * iovecs;
	iovecs = (struct iovec *)alloca(sizeof(struct iovec)*cmsgh->msg_iovlen);
	compat_iov(iovecs, compat_iovecs, cmsgh->msg_iovlen);

	struct msghdr msgh;
	compat_msghdr(&msgh, cmsgh, iovecs);
	ret = __k42_linux_sendmsg(a0, &msgh, a[2]);
	}
	break;

    case SYS_RECVMSG:
	{
	pre_trace32("__k42_linux_recvmsg", a);
	struct compat_msghdr *cmsgh = (struct compat_msghdr *)compat_ptr(a1);
	struct compat_iovec *compat_iovecs = (struct compat_iovec *)compat_ptr(cmsgh->msg_iov);

	struct iovec * iovecs;
	iovecs = (struct iovec *)alloca(sizeof(struct iovec)*cmsgh->msg_iovlen);
	compat_iov(iovecs, compat_iovecs, cmsgh->msg_iovlen);

	struct msghdr msgh;
	compat_msghdr(&msgh, cmsgh, iovecs);
	ret = __k42_linux_recvmsg(a0, &msgh, a[2]);
	}
	break;

    default:
	ret = -EINVAL;
	break;
    }
    post_trace(ret);
//err_printf("%s: call=%d  ret=%d\n", __PRETTY_FUNCTION__, call, ret);
    return ret;
}
