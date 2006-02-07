/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATKernel.C,v 1.14 2001/10/12 20:22:44 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Miscellaneous place for early address transation
 * stuff.
 * Machine dependent part
 * **************************************************************************/

inline SysStatus
HATKernel::unmapRange(uval regionAddr, uval regionSize, VPSet ppset)
{
    SysStatus retvalue;
    retvalue = HATDefaultBase<AllocPinnedGlobalPadded>::unmapRange(
	regionAddr, regionSize, ppset);
    return(retvalue);
}
