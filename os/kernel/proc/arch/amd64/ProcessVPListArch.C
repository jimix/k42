/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessVPListArch.C,v 1.1 2001/04/11 17:18:12 peterson Exp $
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
    /* FIXME -- X86-64 */
    SysStatus rc;
    rc=DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(dspAddrKern,PAGE_SIZE);
    return rc;
}
