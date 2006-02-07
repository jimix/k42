/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: baseFgets.C,v 1.18 2001/11/26 14:14:35 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinux.H>
#include <misc/baseStdio.H>

// Function normally return NULL or outbuf, but we return a SysStatus
SysStatus
baseFgets(char *outbuf, sval n, FileLinuxRef iop)
{
    SysStatus rc;
    sval ncopied = 0;
    char *cr, *buf ;

    rc = DREF(iop)->lock();
    if (_FAILURE(rc)) goto error;

    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(iop)->locked_readAlloc(n-1, buf, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }
    if (_FAILURE(rc)) goto error;

    if ((n = _SGETUVAL(rc)) == 0) { /* if got 0, return */
	DREF(iop)->unLock();
	// FIXME: What is the return value for EOF?
	return 0 ;
    }

    // actually got some data
    if ((cr = (char *)memccpy(outbuf, buf, 0xa, n))) {
	if (!cr) {
	    ncopied = n ;
        } else {
	    ncopied = cr - outbuf ;
	    *cr = '\0';
	}
    } else {
	outbuf[0] = '\0';
    }
    if (ncopied < n) {
	if (ncopied >= 0) {

	    while (1) {
		ThreadWait *tw;
		rc = DREF(iop)->locked_readRealloc(buf, n, ncopied, buf, &tw);
		if (_FAILURE(rc) && tw) {
		    while (!tw->unBlocked()) {
			Scheduler::Block();
		    }
		    tw->destroy();
		    delete tw;
		    tw = NULL;
		} else {
		    break;
		}
	    }

	    if (_FAILURE(rc)) goto error;
	}
    }
    rc = DREF(iop)->locked_readFree(buf);

error:
    DREF(iop)->unLock();
    return rc ;
}
