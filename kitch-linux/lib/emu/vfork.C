/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: vfork.C,v 1.3 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: vfork(2) system call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define vfork __k42_linux_vfork
#define fork __k42_linux_fork
#include <unistd.h>

pid_t
vfork(void)
{
#undef vfork
    // FIXME: For now these are the same
    return fork();
#undef fork
}
