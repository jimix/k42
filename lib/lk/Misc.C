/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Misc.C,v 1.8 2005/06/06 19:01:59 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>

//Don't need kernel-specific features
#undef __KERNEL__

#include <asm/types.h>
#include <sync/BLock.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif
