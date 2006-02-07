/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxSocket.C,v 1.18 2005/05/06 19:31:07 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: emulates socket calls
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "FD.H"
#include <io/IO.H>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <io/FileLinux.H>
#include "linuxEmul.H"
#include <errno.h>

extern "C" int
__k42_linux_getsockopt (int s, int level, int optname, const void *optval,
		        socklen_t* optlen);
int
__k42_linux_getsockopt (int s, int level, int optname, const void *optval,
		        socklen_t* optlen)
{
    FileLinuxRef fileRef;
    SysStatusUval rc;

    SYSCALL_ENTER();

    fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    uval optionlen = (uval)*optlen;
    rc = DREF(fileRef)->getsockopt(level, optname, optval, &optionlen);
    *optlen = (socklen_t)optionlen;
    if (_FAILURE(rc)) {
	/* level 1 is SOL_SOCKET   option 8 is SO_RCVBUF ( def in asm/socket.h) */
	err_printf("getsockopt() failed: fd=%d level=%d option=%d ",
			s, level, optname);
	_SERROR_EPRINT(rc);

	return -_SGENCD(rc);
    } else {
	return _SGETUVAL(rc);
    }
}
