/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccoreusr.C,v 1.1 2001/04/11 17:15:11 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user level implementation of PPC_ASYNC
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ppccore.H>

void
PPC_ASYNC(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    /* FIXME -- X86-64 */
    rc = DispatcherDefault_PPCAsync(xhandle, methnum, targetID);
}
