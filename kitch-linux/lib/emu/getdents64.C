/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getdents64.C,v 1.6 2004/08/02 16:44:02 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: read dir enteries from a file descriptor
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"


// There is no prototype for this since it is an internal
// function. However, there exists a prototype in glibc that we can
// use when we merge.
extern "C" ssize_t __k42_linux_getdents64(int fd, char *buf, size_t nbytes);

ssize_t
__k42_linux_getdents64(int fd, char *buf, size_t nbytes)
{
    SYSCALL_ENTER();

#undef getdents	// Must not interfere with the method below
    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (fileRef == NULL) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    SysStatusUval rc;
    rc = DREF(fileRef)->getDents((struct dirent64 *)buf, (uval)nbytes);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return _SGETUVAL(rc);
    }
}

extern "C" ssize_t
__k42_linux_sys32_getdents(int fd, char *buf, size_t nbytes)
{
    SysStatusUval rc;
    size_t k42bufsize;

    if ((k42bufsize = nbytes) < sizeof(struct dirent64))
	k42bufsize = sizeof(struct dirent64);
    char * new_buf = (char *) alloca(k42bufsize);
    rc = __k42_linux_getdents64(fd, new_buf, k42bufsize);
    if (rc > 0) {
	struct linux_dirent32 {
	    uval32		d_ino;
	    uval32		d_off;
	    unsigned short	d_reclen;
	    char		d_name[1];
	} *de32;
	struct dirent64 * de64;
	ssize_t p = rc;
	de32 = (struct linux_dirent32 *) buf;
	de64 = (struct dirent64 *) new_buf;
	uval32 off32 = 0;
	while (p>0 && // stuff remains in 64-bit dirent buf, and
	     de64->d_reclen <= nbytes-off32){ // space remains in 32bit buffer
	    de32->d_ino = de64->d_ino;
	    char * dest = de32->d_name;
	    char * src = de64->d_name;
	    int c = sizeof(struct linux_dirent32) -1;
		// sub 1 for size of char, counted in loop below
	    // not checking if the name really fits in the buffer, but we
	    // already checked that the entire dirent record fit in the
	    // buffer, thus the name should fit.
	    do {
		*dest++ = *src;
		c++;
	    } while ('\0' != *src++);
	    c = ALIGN_UP(c, sizeof(uval64));
	    de32->d_reclen = c;
	    de32->d_off = off32 += c;

	    p -= de64->d_reclen;
	    de64 = (struct dirent64 *) (new_buf + de64->d_off);
	    de32 = (struct linux_dirent32 *) (buf + de32->d_off);
	}

	rc = off32;
    }

    return rc;
}

strong_alias(__k42_linux_getdents64,__k42_linux_getdents);
