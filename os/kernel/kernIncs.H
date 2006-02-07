#ifdef __KERN_INCS_H_
#error <kernIncs.H> should be included exactly once from every kernel .C file.
#else
#define __KERN_INCS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernIncs.H,v 1.20 2004/07/11 21:59:26 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Standard includes assumed everywhere in Kernel
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/ObjectRefs.H>
#include "ObjectRefsKern.H"
#include <bilge/CObjGlobalsKern.H>
#include <cobj/TypeMgr.H>

#endif /* #ifndef __KERN_INCS_H_ */
