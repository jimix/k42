/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SMTMgr.C,v 1.4 2002/10/10 13:08:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared-Memory Transport Manager manages connections
 *		       of MemTrans objects between processes
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SMTMgr.H"
#include <sys/ProcessSet.H>
#include <misc/AutoList.I>


SysStatus
SMTMgr::getMemTrans(ObjectHandle &oh,
		    uval key,
		    XHandle xhandle,
		    ProcessID pid)
{
    SysStatus rc;
    MemTransRef mtr;
    BaseProcessRef pref;

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid,pref);

    _IF_FAILURE_RET(rc);

    rc = DREF(pref)->getLocalSMT(mtr, xhandle, key);

    _IF_FAILURE_RET(rc);

    rc = DREF(mtr)->getOH(pid,oh);

    //Let go of the reference
    DREF(mtr)->detach();
    return rc;
}

SysStatus
SMTMgr::getLocalSMT(MemTransRef	&mtr,
		    XHandle	&remote,
		    uval	key)
{

    SMTHolder *cur = NULL;
    SysStatus rc;
    do {
	rc = 0;
	mtr = NULL;
	holderList.lock();
	// Look for a match in the list
	for (cur = (SMTHolder*)holderList.next(); cur;
	    cur = (SMTHolder*)cur->next()) {

	    if (cur->key == key) {
		break;
	    }

	}
	if (cur) {
	    mtr = cur->mtr;
	}

	//Increase the ref count
	//This could fail and we have to start the search over again
	//Once incRefCount succeeds, we know mtr won't disappear
	if (mtr) {
	    rc = DREF(mtr)->incRefCount();
	}

    } while (cur  && _FAILURE(rc) && _SGENCD(rc)==EBUSY);


    if (!cur) {
	holderList.unlock();
	return _SERROR(2009, 0, ENOENT);
    }

    if (!cur->remote) {
	cur->remote = remote;
    } else {
	remote = cur->remote;
    }

    holderList.unlock();

    return 0;
}


SysStatus
SMTMgr::removeSMT(MemTransRef mtr)
{
    SMTHolder *cur = NULL;

    holderList.lock();
    // Look for a match in the list
    for (cur = (SMTHolder*)holderList.next(); cur;
	cur = (SMTHolder*)cur->next()) {

	if (cur->mtr == mtr) {
	    cur->lockedDetach();
	    delete cur;
	    break;
	}
    }

    holderList.unlock();
    return 0;
}

SysStatus
SMTMgr::addSMT(MemTransRef mtr, XHandle remoteSMT, uval key)
{
    SMTHolder *cur = NULL;

    holderList.lock();
    // Look for a match in the list
    for (cur = (SMTHolder*)holderList.next(); cur;
	cur = (SMTHolder*)cur->next()) {

	if (cur->key == key) {
	    holderList.unlock();
	    return _SERROR(2012, 0, EALREADY);
	}

    }

    cur = new SMTHolder(mtr, remoteSMT, key);
    holderList.lockedAppend(cur);
    holderList.unlock();
    return 0;
}
