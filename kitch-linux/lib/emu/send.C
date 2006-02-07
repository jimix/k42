/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: send.C,v 1.6 2004/06/14 20:32:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define send __k42_linux_send
#define sendto __k42_linux_sendto
#include <sys/socket.h>
#undef send
#undef sendto

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#if !defined(GCC3) && !defined(TARGET_amd64) /* actually PLATFORM_Linux */
int
#else /* #ifndef TARGET_amd64 */
ssize_t
#endif /* #ifndef TARGET_amd64 */
__k42_linux_send(int s, const void *msg, size_t len, int flags)
{
    const struct sockaddr *to = NULL;
    socklen_t tolen = 0;
    return __k42_linux_sendto(s, msg, len, flags, to, tolen);
}

