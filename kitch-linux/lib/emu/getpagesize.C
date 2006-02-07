/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getpagesize.C,v 1.8 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for get the systems page size.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include <alloc/PageAllocatorUser.H>
#include "linuxEmul.H"

#define getpagesize __k42_linux_getpagesize
#include <unistd.h>

/* inline */ int
getpagesize (void)
{
    return(PAGE_SIZE);
}
