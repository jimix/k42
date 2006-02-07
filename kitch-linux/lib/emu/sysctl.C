/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysctl.C,v 1.1 2004/08/30 19:59:52 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"
#include <linux/sysctl.h>


#define UTS_VERSION_1   "#1 SMP "
#define UTS_VERSION     UTS_VERSION_1 COMPILE_DATE
// COMPILE_DATE is set in the makefile
const char *linux_uts_version = UTS_VERSION "\n";

int
trace_unimplemented_sysctl(struct __sysctl_args *args)
{
    int i;
    
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	for ( i = 0; i < args->nlen; i++)
	    err_printf("Warning: _sysctl: name %d\n", args->name[i]);
    }

    return -ENOTDIR;
}

SysStatus
sysctl_out(const char *str, struct __sysctl_args *args)
{
    if (0 == *(args->oldlenp) ) return -EFAULT;
    /* this does not copy the null terminating character.  Should it?  For
	caller buffers that are smaller that str, linux fills in as many
	characters as it can without terminating the string. */
    int len = strlen(str) + 1;
    if ((size_t)len > *(args->oldlenp) ) {
	len = *(args->oldlenp);
	memcpy(args->oldval, str, len);
	*(args->oldlenp) = (size_t) len;
    } else {
	memcpy(args->oldval, str, len);
	*(args->oldlenp) = (size_t) (len - 1);
    }
    return 0;
}

extern "C" int
__k42_linux__sysctl(struct __sysctl_args *args)
{
    SysStatus rc;

    if(args->nlen < 1)
	return -EINVAL;

    switch(args->name[0]) {
    case CTL_KERN:
	if(args->nlen < 2)
	    return -ENOTDIR;

	switch(args->name[1]){
	case KERN_VERSION:
	    rc = 0;
	    if(args->oldval) // trying to read
		rc = sysctl_out(linux_uts_version, args);
	    if(args->newval)  // trying to write
		return -EPERM;

	    return rc;
	default:
	    return trace_unimplemented_sysctl(args);
	}
	break;

    default:
	return trace_unimplemented_sysctl(args);
    }
    
    tassert(0, err_printf("%s: not reachable: ", __PRETTY_FUNCTION__));
}

extern "C" int
__k42_linux_sys32_sysctl(struct __sysctl_args *args)
{
    SysStatus rc;
    rc = (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, ENOSYS));
    return rc;
}
