#ifndef __LINKAGE_H_
#define __LINKAGE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linkage.H,v 1.8 2001/10/05 21:47:52 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions and services related to linkage
 *                     conventions
 * **************************************************************************/

/*
 * Get as much of call chain as is asked for.  Unneeded entries are cleared.
 * Call by passing an initial frame pointer, an array of uvals, and the number.
 */
SysStatus GetCallChain(uval startFrame, uval* callChain, uval callCount);
SysStatus GetCallChainSelf(uval skip, uval* callChain, uval callCount);
#endif /* #ifndef __LINKAGE_H_ */
