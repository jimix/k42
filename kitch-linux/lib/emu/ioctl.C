/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ioctl.C,v 1.22 2004/08/30 19:58:36 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: ioctl - control device
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <cobj/sys/COSMgr.H>
#include <sys/ProcessWrapper.H>

// #define ioctl __k42_linux_ioctl NOOP, since no prototype
#include <linux/termios.h>
#include <linux/ioctl.h>
// #undef ioctl

/*
 * when we include the header for the linux/ioctl.h, it doesnt define
 * the prototype for ioctl, so we have to explicitly say extern "C" here
 * if we include the sys/ioctl.h, then it uses the glibc and not the linux
 * information, and since ioctl is converted by glibc, this is bad.
 */ 

extern "C" int
__k42_linux_ioctl(int fd, unsigned long request, ...)
{
    SYSCALL_ENTER();

    SysStatus rc;
    va_list args;

    FileLinuxRef flRef = _FD::GetFD(fd);

    /* is the descriptor good? */
    if (flRef == NULL) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    va_start(args, request);
    rc = DREF(flRef)->ioctl(request, args);
    va_end(args);

    SYSCALL_EXIT();
    if (_SUCCESS(rc)) {
	return 0;
    }
    
    return -_SGENCD(rc);
}

/*
 * First quick hack, adding ioctls as they are called by the 32 bit code, 
 * one by one looking at Linux code for mapping.  We pretty quickly want to 
 * convert (once we have more than ten or so) to using the Linux implementation 
 * directly, should be pretty easy to at least grab the hash table idea 
 * and the list of compatible iocts... from 2.6 Linux code. 
 *
 * Things to look at from Linux 2.6 code are:
 * 
 * list of compatible ioctl commands that are generic:
 * LINUXTREE/include/linux/compat_ioctl.h
 * 
 * the aditional PPC rountines that build hash table..
 * LINUXTREE/arch/ppc64/kernel/ioctl32.c
 *  
 * the generic routines that do the conversion
 * LINUXTREE/fs/compat.c <- actual routines
 * 
 * the glibc conversion that translates the operations that structures changexsy
 * /u/kitchawa/k42/src/glibc/sysdeps/unix/sysv/linux/powerpc/ioctl.c
 */

extern "C" int
__k42_linux_ioctl_32(int fd, unsigned long request, ...)
{
    int sysrc;
    SYSCALL_ENTER();

    SysStatus rc;
    va_list args;

    FileLinuxRef flRef = _FD::GetFD(fd);

    /* is the descriptor good? */
    if (flRef == NULL) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    switch (request) {
    default:
	err_printf("IOCTL: DIR = ");
	switch (_IOC_DIR(request)) {
	case _IOC_NONE:
	    err_printf("NONE ");
	    break;
	case _IOC_READ:
	    err_printf("READ ");
	    break;
	case _IOC_WRITE:
	    err_printf("WRITE ");
	    break;
	case _IOC_WRITE|_IOC_READ:
	    err_printf("READ/WRITE ");
	    break;
	default:
	    err_printf("??? ");
	};
	err_printf("REQUEST %x (c-%c) ",  (unsigned int)_IOC_TYPE(request),  
		   (unsigned char)_IOC_TYPE(request));
	err_printf("IOC_NR %x (d-%d)",  (unsigned int)_IOC_NR(request),  
		   (unsigned int)_IOC_NR(request));
	err_printf("IOC_SIZE %x (d-%d)\n",  (unsigned int)_IOC_SIZE(request), 
		   (unsigned int)_IOC_SIZE(request));
    /* fall though intentionally, which is don't print in the cases below */
    case TCGETS:
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TIOCGWINSZ:
    case TIOCSWINSZ:
    case TIOCGPGRP:
    case TIOCSPGRP:

	/* gets passed right through even if it might be wrong*/
	va_start(args, request);
	rc = DREF(flRef)->ioctl(request, args);
	va_end(args);
	if (_SUCCESS(rc)) { 
	    sysrc = 0; 
	} else {
	    sysrc = -_SGENCD(rc);
	}
    }

    SYSCALL_EXIT();
    return sysrc;
}

