#ifndef __USE_EXPEDIENT_H_
#define __USE_EXPEDIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: use_expedient.H,v 1.13 2003/11/13 15:32:20 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Controls whether to use expedient or optimized
 *                      implementations of various Dispatcher- and
 *                      Exception-level paths.
 * **************************************************************************/

#if defined(TARGET_powerpc)
#undef USE_EXPEDIENT_USER_PGFLT
#undef USE_EXPEDIENT_PPC
#undef USE_EXPEDIENT_SCHEDULER
#undef USE_EXPEDIENT_RESERVED_THREAD
#undef USE_EXPEDIENT_USER_RESUME
#undef USE_EXPEDIENT_INTERRUPT
#undef USE_EXPEDIENT_SVC

#elif defined(TARGET_mips64)
#undef USE_EXPEDIENT_USER_PGFLT
#undef USE_EXPEDIENT_PPC
#undef USE_EXPEDIENT_SCHEDULER
#undef USE_EXPEDIENT_RESERVED_THREAD
#undef USE_EXPEDIENT_USER_RESUME
#undef USE_EXPEDIENT_INTERRUPT
#define USE_EXPEDIENT_SVC

#elif defined(TARGET_amd64)
#define USE_EXPEDIENT_USER_PGFLT
#define USE_EXPEDIENT_PPC
#define USE_EXPEDIENT_SCHEDULER
#define USE_EXPEDIENT_RESERVED_THREAD
#define USE_EXPEDIENT_USER_RESUME
#define USE_EXPEDIENT_INTERRUPT
#define USE_EXPEDIENT_SVC

#elif defined(TARGET_generic64)
#define USE_EXPEDIENT_USER_PGFLT
#define USE_EXPEDIENT_PPC
#define USE_EXPEDIENT_SCHEDULER
#define USE_EXPEDIENT_RESERVED_THREAD
#define USE_EXPEDIENT_USER_RESUME
#define USE_EXPEDIENT_INTERRUPT
#define USE_EXPEDIENT_SVC

#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

#endif /* #ifndef __USE_EXPEDIENT_H_ */
