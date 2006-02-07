/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessVPListArch.C,v 1.6 2005/07/18 21:49:20 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include "ProcessVPList.H"
#include "mem/PageAllocatorKernPinned.H"

SysStatus
ProcessVPList::archAllocDispatcherPage(uval /*dspAddr*/, uval &dspAddrKern)
{
    if (exceptionLocal.realModeMemMgr != NULL) {
	exceptionLocal.realModeMemMgr->alloc(dspAddrKern, PAGE_SIZE, PAGE_SIZE);
	if (dspAddrKern == 0) {
	    return _SERROR(2928, 0, ENOMEM);
	}
	return 0;
    } else {
	SysStatus rc;
	rc=DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(dspAddrKern,
							    PAGE_SIZE);
	return rc;
    }
}
