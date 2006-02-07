#ifndef __FSCREDS_H_
#define __FSCREDS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSCreds.H,v 1.8 2005/09/06 20:40:12 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Defines the fundamental interface to object that
 * maintains name space for other objects
 ****************************************************************************/

#include <sys/ProcessLinux.H>

class FSCreds {
public:
    static SysStatus Acquire(ProcessID pid) {
#ifdef NO_CREDENTIAL_CHECKING
	return 0;
#else
	SysStatus rc;
	ProcessLinux::creds_t* creds;

	rc = DREFGOBJ(TheProcessLinuxRef)->getCredsPointerNativePid(pid,
								    creds);
	tassertMsg(_SUCCESS(rc),"bad pid 0x%lx\n", pid);
	if (_FAILURE(rc)) return rc;
	Scheduler::SetThreadSpecificUvalSelf((uval)creds);
	return 0;
#endif // #ifdef NO_CREDENTIAL_CHECKING
    }

    static void Release() {
#ifdef NO_CREDENTIAL_CHECKING
#else
	ProcessLinux::creds_t* creds;
	creds = (ProcessLinux::creds_t*)Scheduler::GetThreadSpecificUvalSelf();
	DREFGOBJ(TheProcessLinuxRef)->releaseCredsPointer(creds);
	Scheduler::SetThreadSpecificUvalSelf((uval)0); // to catch errors
#endif // #ifdef NO_CREDENTIAL_CHECKING
    }

    static ProcessLinux::creds_t *Get(uval assert = 1) {
#ifdef NO_CREDENTIAL_CHECKING
	passertMsg(0, "why are we here?\n");
	return NULL;
#else
	ProcessLinux::creds_t* creds;
	creds = (ProcessLinux::creds_t*)Scheduler::GetThreadSpecificUvalSelf();
	tassertMsg( assert == 0 || creds != NULL, "uninitialized credentials\n");
	return creds;
#endif // #ifdef NO_CREDENTIAL_CHECKING
    }
};
#endif /* #ifndef __FSCREDS_H_ */
