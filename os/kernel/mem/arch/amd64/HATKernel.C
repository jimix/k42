/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATKernel.C,v 1.2 2001/10/12 18:42:20 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Miscellaneous place for early address transation
 * stuff.
 * Machine dependent part
 * **************************************************************************/

// VVV XXX probably ok for amd64

inline SysStatus
HATKernel::unmapRange(uval regionAddr, uval regionSize, VPSet ppset)
{
    SysStatus retvalue;
    retvalue = HATDefaultBase<AllocPinnedGlobalPadded>::unmapRange(
	regionAddr, regionSize, ppset);
    return(retvalue);
}
