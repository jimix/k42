/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxEmul.C,v 1.77 2004/07/01 21:14:20 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base implementation of linux write
 * **************************************************************************/

/* FIXME
 * should really generate this Automajikally
 */
#include <sys/sysIncs.H>
#include <stdio.h>
#include "linuxEmul.H"
#include "defines/linux_support.H"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __NO_LINUX_IMPL_WARNINGS
// ask env NOSUPP
static int __k42_linux_reportNoSupport = -1;
#else
// Report NoSupport
static int __k42_linux_reportNoSupport = 1;
#endif

sval
__k42_linux_emulNoSupport (const char *func, sval ret)
{
    SYSCALL_ENTER();

    switch (__k42_linux_reportNoSupport) {
    default:
    case 0:
	// no report
	break;
    case -1:
	// check K42 shell environment (only if it exists)
// FIXME:
// getenv is glibc, which we may not have early on
//	if (getenv("NOSUPP") == NULL) {
	    __k42_linux_reportNoSupport = 0;
	    break;
//	}
//	__k42_linux_reportNoSupport = 1;
	/*FALLTHRU*/
    case 1:
	err_printf("\n*** No support for %s", func);
	err_printf("... continuing with return (%ld)", ret);
	err_printf("\n");
	break;
    }
    SYSCALL_EXIT();
    return (ret);
}

#ifdef __cplusplus
}
#endif
