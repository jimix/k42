#ifdef __SYS_INCS_H_
#error <sys/sysIncs.H> should be included exactly once from every .C file.
#else
#define __SYS_INCS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysIncs.H,v 1.40 2004/07/11 21:59:25 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Standard includes assumed in any system program
 * **************************************************************************/

//FIXME g++ is current defining NULL to be (void *)0
#undef NULL
#define NULL 0

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* #ifndef _GNU_SOURCE */

#include <sys/kinclude.H>
#include <sys/config.H>
#include <sys/macdefs.H>
#include <misc/macros.H>
#include <misc/asmdef.h>
#include <sys/types.H>
#include <string.h>
#include <stdarg.h>
#include <sys/TAssert.H>
// following are implicitly included from Obj.H
//  #include <sys/kinclude.H>
//  #include <sys/sysTypes.H>
//  #include <sys/SysStatus.H>
//  #include <cobj/ObjectRefs.H>
//  #include <cobj/ObjectHandle.H>
//  #include <errno.h>
//  #include <sync/Lock.H>
#include <cobj/Obj.H>
#include <misc/utilities.H>
#include <alloc/alloc.H>
#include <cobj/CObjGlobals.H>
#include <misc/BitStructure.H>
#include <misc/StringShort.H>
#include <sys/BaseProcess.H>
#include <sys/ProcessServer.H>

// FIXME do we need this for gcc-3 still?? -JX
#define alloca(size)	__builtin_alloca (size)

#endif /* #ifndef __SYS_INCS_H_ */
