/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: umask.C,v 1.2 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: We hang on to umask in our own address space
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#include <sys/types.h>
#define umask __k42_linux_umask
#include <sys/stat.h>

mode_t
umask(mode_t mask)
{
#undef umask	// Must not interfere with below
    return ((mode_t)FileLinux::SetUMask((uval) mask));
}
