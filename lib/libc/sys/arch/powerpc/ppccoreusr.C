/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccoreusr.C,v 1.4 2000/08/02 19:02:48 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user level implementation of PPC_ASYNC
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ppccore.H>

void
PPC_ASYNC(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    rc = DispatcherDefault_PPCAsync(xhandle, methnum, targetID);
}
