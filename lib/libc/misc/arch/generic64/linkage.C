/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linkage.C,v 1.1 2001/06/12 21:52:58 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions and services related to linkage
 *                     conventions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linkage.H"

SysStatus
GetCallChain(uval startFrame, uval* callChain, uval callCount)
{
  return 0;
}

SysStatus
GetCallChainSelf(uval skip, uval* callChain, uval callCount)
{
  return 0;
}


