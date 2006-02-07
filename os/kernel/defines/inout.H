#ifndef __INOUT_H_
#define __INOUT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: inout.H,v 1.22 2005/02/16 00:06:07 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines what input output devices should be used
 * **************************************************************************/

#include <sys/KernelInfo.H>

#if defined(TARGET_powerpc)
#define THINWIRE_CONSOLE_ENABLED (!KernelInfo::OnSim())
#elif defined(TARGET_mips64)
#define THINWIRE_CONSOLE_ENABLED 0
#elif defined(TARGET_amd64)
#define THINWIRE_CONSOLE_ENABLED 1
#elif defined(TARGET_generic64)
#define THINWIRE_CONSOLE_ENABLED 0
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */


#endif /* #ifndef __INOUT_H_ */
