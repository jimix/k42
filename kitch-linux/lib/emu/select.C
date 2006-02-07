/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: select.C,v 1.27 2005/06/06 13:30:26 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of select
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <unistd.h>
#include <sys/poll.h>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <alloc/alloc.H>

#include <sys/SystemMiscWrapper.H>

int
__k42_linux__newselect (int n, fd_set *readfds,
                        fd_set *writefds,
                        fd_set *exceptfds,
                        struct timeval *tv)
{
    SYSCALL_ENTER();
    SysStatus rc;
    int ret;
    if (n < 0) {
	ret = -EINVAL;
	SYSCALL_EXIT();
	return ret;
    }

    uval i = 0;
    _FD::FDSet *r,*w,*x;
    bool touched;
    uval idx = 0;
    sval timeout = 0;

    pollfd *pfd;

    fd_set old_r, old_w, old_x;

    if (n < 64) {
	pfd = (pollfd*)alloca(sizeof(pollfd) * n);
    } else {
	pfd = (pollfd*)allocGlobal(sizeof(pollfd) * n);
    }

    r = (_FD::FDSet *)readfds;
    w = (_FD::FDSet *)writefds;
    x = (_FD::FDSet *)exceptfds;

    while (i < (unsigned)n) {
	touched = false;
	pfd[idx].events = 0;
	if (r && r->isSet(i)) {
	    touched = true;
	    pfd[idx].events |= FileLinux::POLLRDNORM | FileLinux::POLLRDBAND
				| FileLinux::READ_AVAIL | FileLinux::HUP
				| FileLinux::EXCPT_SET;
	}

	if (w && w->isSet(i)) {
	    touched = true;
	    pfd[idx].events |= FileLinux::POLLWRBAND | FileLinux::POLLWRNORM |
		FileLinux::WRITE_AVAIL | FileLinux::EXCPT_SET;
	}

	if (x && x->isSet(i)) {
	    touched = true;
	    pfd[idx].events |= FileLinux::POLLPRI;
	}

	if (touched) {
	    pfd[idx].fd = i;
	    ++idx;
	}
	++i;
    }

    if (r) memcpy((void*) &old_r, (void*)r, sizeof(fd_set));
    if (w) memcpy((void*) &old_w, (void*)w, sizeof(fd_set));
    if (x) memcpy((void*) &old_x, (void*)x, sizeof(fd_set));

    do {
	if (tv) {
	    timeout = tv->tv_usec + tv->tv_sec * 1000000;
	    rc = _FD::Poll(pfd, idx, timeout);
	    if (timeout < 0) {
		tv->tv_usec = tv->tv_sec = 0;
	    } else {
		tv->tv_usec = timeout % 1000000;
		tv->tv_sec  = timeout / 1000000;
	    }

	} else {
	    timeout = -1;
	    rc = _FD::Poll(pfd, idx, timeout);
	}
        SysStatus pollrc = rc;  // save it to for debugging purposes


	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	    if (n >= 64) {
	        freeGlobal(pfd, sizeof(pollfd) * n);
	    }
	    SYSCALL_EXIT();
	    return ret;
	}

	i = 0;
	rc = 0;

	if (r) r->zero(n);
	if (w) w->zero(n);
	if (x) x->zero(n);

	while (i < idx) {
	    touched = false;
	    if (r && FD_ISSET(pfd[i].fd, &old_r) &&
		pfd[i].revents & (FileLinux::POLLRDNORM |
				  FileLinux::POLLRDBAND |
			          FileLinux::READ_AVAIL |
				  FileLinux::HUP |
			          FileLinux::EXCPT_SET  |
				  FileLinux::ENDOFFILE)) {
		touched = true;
		r->set(pfd[i].fd);
	    }

	    if (w && FD_ISSET(pfd[i].fd, &old_w) && 
		pfd[i].revents & (FileLinux::POLLWRNORM |
				  FileLinux::POLLWRBAND |
			          FileLinux::WRITE_AVAIL |
				  FileLinux::EXCPT_SET)) {
		touched = true;
		w->set(pfd[i].fd);
	    }

	    if (x && /* FD_ISSET(pfd[i].fd, &old_x) && */
		pfd[i].revents & FileLinux::POLLPRI) {
		touched = true;
		x->set(pfd[i].fd);
		tassertMsg(FD_ISSET(pfd[i].fd, &old_x),
			   "exep not set (pollrc %lx)\n", pollrc);
	    }

	    if (touched) ++rc;
	    ++i;
	}

	tassertMsg(((rc!=0)||(tv!=NULL)), "looping for 0 rc and NULL timeout\n");
    } while ((rc==0)&&(tv==NULL));

    if (n >= 64) {
	freeGlobal(pfd, sizeof(pollfd) * n);
    }

    SYSCALL_EXIT();
    tassertMsg(((rc!=0)||(tv!=NULL)), "0 rc with null timeout");

    return rc;
}

typedef uval32 __fd_mask32;

struct fd_set32 {
    __fd_mask32 fds_bits[16];
};

struct timeval32 {
    sval32 tv_sec;
    sval32 tv_usec;
};

/* (gdb) ptype fd_set
 * type = struct {
 *   __fd_mask fds_bits[16];
 * }
 * (gdb) ptype __fd_mask
 * type = long int
 * (gdb64) p sizeof(__fd_mask)
 * $1 = 8
 * (gdb32) p sizeof(__fd_mask)
 * $1 = 4
 */
static int fds32_to_fds64(const fd_set32 *fds32, fd_set *fds64)
{
    int i, j;

    /* We are assuming a big-endian architecture here.  */
    for (i = 0, j = 0; i < 8; i++, j += 2) {
	fds64->fds_bits[i] = ZERO_EXT(fds32->fds_bits[j]) |
	  (ZERO_EXT(fds32->fds_bits[j + 1]) << 32);
    }

    /* Just make the last eight longs zero.  */
    for (i = 8; i < 16; i++) {
	fds64->fds_bits[i] = 0;
    }

    return 0;
}

/* We are assuming a big-endian architecture here.  */
static int fds64_to_fds32(const fd_set *fds64, fd_set32 *fds32)
{
    for (int i = 0; i < 16; i +=2) {
	fds32->fds_bits[i] = fds64->fds_bits[i / 2] & 0xFFFFFFFF;
	fds32->fds_bits[i + 1] = (fds64->fds_bits[i / 2] >> 32) & 0xFFFFFFFF;
    }

    return 0;
}

extern "C" sval32
__k42_linux__newselect_32 (sval32 n, fd_set32 *rfds, fd_set32 *wfds,
			   fd_set32 *efds, struct timeval32 *tv)
{
    sval32 ret;
    fd_set rfds64, wfds64, efds64;
    struct timeval tv64;

    if (rfds != NULL) fds32_to_fds64(rfds, &rfds64);
    if (wfds != NULL) fds32_to_fds64(wfds, &wfds64);
    if (efds != NULL) fds32_to_fds64(efds, &efds64);

    if (tv != NULL) {
	tv64.tv_sec = tv->tv_sec;
	tv64.tv_usec = tv->tv_usec;
    }

    ret = __k42_linux__newselect(n,
				 rfds ? &rfds64 : NULL, wfds ? &wfds64 : NULL,
				 efds ? &efds64 : NULL, tv ? &tv64 : NULL);

    if (rfds != NULL) fds64_to_fds32(&rfds64, rfds);
    if (wfds != NULL) fds64_to_fds32(&wfds64, wfds);
    if (efds != NULL) fds64_to_fds32(&efds64, efds);

    if (tv != NULL) {
	tv->tv_sec = tv64.tv_sec;
	tv->tv_usec = tv64.tv_usec;
    }

    return ret;
}
