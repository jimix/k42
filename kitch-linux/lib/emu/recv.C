/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: recv.C,v 1.7 2004/06/14 20:32:55 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define recv __k42_linux_recv
#define recvfrom __k42_linux_recvfrom
#include <sys/socket.h>
#undef recv
#undef recvfrom

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/Socket.H>

#if !defined(GCC3) && !defined(TARGET_amd64) /* actually PLATFORM_Linux */
int
#else /* TARGET_amd64 */
ssize_t
#endif /* TARGET_amd64 */
__k42_linux_recv(int s, void *buf, size_t len, int flags)
{
    // The recv call is identical to recvfrom with a NULL from
    // parameter.

    // FIXME: this us fugly but michal promises that and saddr is
    // never > 256 bytes.. should really fix this on the other side
    struct sockaddr *ignoreSock = (struct sockaddr *) alloca(256);
    socklen_t ignoreLen = 0;

    return __k42_linux_recvfrom(s, buf, len, flags, ignoreSock, &ignoreLen);
}
