/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: socket.C,v 1.18 2005/04/20 13:31:28 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define socket __k42_linux_socket
#include <sys/socket.h>
#undef socket
#undef socketpair

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#undef VERBOSE_SOCKET

extern "C" 
int
__k42_linux_socket(int domain, int type, int protocol);

int
__k42_linux_socket(int domain, int type, int protocol)
{
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef newFileRef;
    rc = FileLinux::Socket(newFileRef,domain,type,protocol);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	#ifdef VERBOSE_SOCKET
	_SERROR_EPRINT(rc);
	#endif // ifdef VERBOSE_SOCKET
	return -_SGENCD(rc);
    }
    int ret = _FD::AllocFD(newFileRef);
    SYSCALL_EXIT();
    return ret;

}

extern "C"
int
__k42_linux_socketpair (int domain, int type, int protocol, int sv[2]);

int
__k42_linux_socketpair (int domain, int type, int protocol, int sv[2])
{
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef newFileRef1, newFileRef2;
    rc = FileLinux::SocketPair(newFileRef1,newFileRef2, domain, type, protocol);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    sv[0] = _FD::AllocFD(newFileRef1);
    sv[1] = _FD::AllocFD(newFileRef2);
    SYSCALL_EXIT();
    return 0;
}
