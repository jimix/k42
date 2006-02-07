/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Pin.C,v 1.7 2005/01/10 15:28:15 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Utilities to handle pinning of pages for IO
 * **************************************************************************/

#include <kernIncs.H>
#include "Pin.H"
#include <proc/Process.H>

// Takes iovec "in" in process "pref" and fills in
// iovec "vec" to refer to pinned memory for said address range.
// length of vec and pages arrays must be equal to number of pages
// spanned by "in"
// pages array must be 0 filled

SysStatusUval
PinnedMapping::pinAddr(ProcessRef pref, uval inAddr, uval &outAddr,
		       uval writeable, uval flags)
{
    tassertMsg(addr == NULL, "??\n");

    SysStatus rc = 0;
    VPNum vp = Scheduler::GetVP();
    uval a = inAddr & ~PAGE_MASK;

    rc = DREF(pref)->vaddrToRegion(a, rr);
    _IF_FAILURE_RET(rc);

    rc = DREF(rr)->vaddrToFCM(vp, a, writeable, fcm, fcmOffset);
    _IF_FAILURE_RET(rc);

    rc = DREF(fcm)->getPage(fcmOffset, addr, NULL);

    if (_FAILURE(rc)) {
	goto unpin;
    }

    outAddr = (uval)addr+(inAddr & PAGE_MASK);

    return 0;
  unpin:
    SysStatus err=0;
    if (addr) {
	err = DREF(fcm)->releasePage(fcmOffset);
	tassertMsg(_SUCCESS(err),"Release should succeed: %lx "
		   "(addr %p)\n", err, addr);
    }
    return rc;

}

/*static */ SysStatusUval
PinnedMapping::pinToIOVec(ProcessRef pref, struct iovec *in, uval veclen,
			  uval writeable, PinnedMapping *pages,
			  struct iovec *vec, uval flags)
{
    SysStatus rc = 0;
    uval i = 0;
    for (uval x = 0; x<veclen; ++x) {
	uval end = (uval)in[x].iov_base + in[x].iov_len;
	uval start = (uval)in[x].iov_base;


	if ((flags & FullPagesOnly)  &&
	   ((start & PAGE_MASK) || (end & PAGE_MASK))) {
	    rc = _SERROR(2245, 0, EINVAL);
	    goto unpin;
	}

	while (start<end) {
	    uval pinned;
	    rc = pages[i].pinAddr(pref, start, pinned, writeable,  flags);
	    if (_FAILURE(rc)) {
		goto unpin;
	    }

	    //Vec element from addr to end of page
	    vec[i].iov_base = (void*)pinned;
	    if ((start & ~PAGE_MASK)  == (end & ~PAGE_MASK)) {
		vec[i].iov_len = end - start;
	    } else {
		//To end of current page
		vec[i].iov_len = PAGE_SIZE - (start&PAGE_MASK);
	    }

	    start = (PAGE_SIZE + start) & ~PAGE_MASK;
	    ++i;
	}
    }
    return i;

 unpin:
    SysStatus err;

    while (i>0) {
	--i;
	if (pages[i].addr) {
	    err = DREF(pages[i].fcm)->releasePage(pages[i].fcmOffset);
	    tassertMsg(_SUCCESS(err),"Release should succeed: %lx\n",err);
	}
    }
    return rc;

}

