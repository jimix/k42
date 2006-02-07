/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getpmsg.C,v 1.1 2004/06/23 20:25:09 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <stropts.h>
#include <errno.h>

extern "C" int
__k42_linux_getpmsg (int fildes, struct strbuf *ctlptr, struct strbuf *dataptr,
		     int *bandp, int *flagsp)
{
    return -ENOSYS;
}

