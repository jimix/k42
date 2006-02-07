/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: uname.C,v 1.19 2005/02/23 16:20:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <stub/StubKBootParms.H>
#include "linuxEmul.H"
#include <fcntl.h>
#define uname __k42_linux_uname
#include <sys/utsname.h>
#undef uname
#define read __k42_linux_read
#define close __k42_linux_close
#include <unistd.h>

struct utsname utsname_data = { {0,},};

int
__k42_linux_uname (struct utsname *ptr)
{
    if ('\0' == utsname_data.sysname[0]) {
	strcpy(utsname_data.sysname, "Linux");
	strcpy(utsname_data.version, "#K42");
	strcpy(utsname_data.machine, "ppc64");
    }

    if ('\0' == utsname_data.release[0]) {
	char buf[_UTSNAME_RELEASE_LENGTH]={0,};
        SYSCALL_ENTER();
	StubProcessLinuxServer::_getLinuxVersion(buf, _UTSNAME_RELEASE_LENGTH);
	strncpy(utsname_data.release, buf, _UTSNAME_RELEASE_LENGTH);
	SYSCALL_EXIT();
    }

    if ('\0' == utsname_data.nodename[0]) {
	char buf[257]={0,};
        SYSCALL_ENTER();
	if (_SUCCESS(StubKBootParms::_GetParameterValue("K42_IP_HOSTNAME",
							 buf, 257))
	    && buf[0] != 0) {
	    char *domain;
	    domain = &buf[0];
	    while (*domain != '.' && *domain != '\0') {
		++domain;
	    }
	    if (*domain == '.') {
		*domain = 0;
		++domain;
	    }
	    strncpy(&utsname_data.nodename[0], buf, _UTSNAME_DOMAIN_LENGTH);
#ifdef __USE_GNU
	    strncpy(&utsname_data.domainname[0], domain, _UTSNAME_DOMAIN_LENGTH);
#else
	    strncpy(&utsname_data.__domainname[0], domain, _UTSNAME_DOMAIN_LENGTH);
#endif /* __USE_GNU */
	} else {
	    char *tmp, *to;

	    tmp = "localhost";
	    to = ptr->nodename;
	    memcpy(to, tmp, strlen(tmp)+1);

	    tmp = "watson.ibm.com";
#ifdef __USE_GNU
	    to = ptr->domainname;
#else
	    to = ptr->__domainname;
#endif
	    memcpy(to, tmp, strlen(tmp)+1);
	}
	SYSCALL_EXIT();
    }

    memcpy(ptr, &utsname_data, sizeof(utsname_data));
    return 0;
}


extern "C" int
__k42_linux_uname_32 (struct utsname *ptr)
{
    sval32 rc = __k42_linux_uname(ptr);

    if (rc==0) {
       memcpy(ptr->machine, "ppc\0\0", 8);
    }
    return rc;
}
