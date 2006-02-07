/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFacts.C,v 1.3 2001/09/20 00:24:18 pdb Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernPinned.H"
#include "exception/ExceptionLocal.H"
// #include __MINC(bios_client.H)

// FIXME:  See fix me in x86/SysFacts.H re apicId
uval8								// XXX seems enuff but CHECK pdb 
SysFacts::procId(const uval proc)
{
    return phyProc[proc].id;
}

/*static*/ void
SysFacts::GetRebootImage(uval &imageAddr, uval &imageSize)
{
    // no fast reboot image on amd64... yet
    imageAddr = 0;
    imageSize = 0;
}

