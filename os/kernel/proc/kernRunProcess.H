#ifndef __KERN_RUN_PROCESS_H_
#define __KERN_RUN_PROCESS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernRunProcess.H,v 1.24 2003/10/14 17:56:07 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines routines exported by runUserProc.C
 * **************************************************************************/

#include <io/PathName.H>
extern SysStatus StartDefaultServers();
extern SysStatus kernRunInternalProcess(char *name, char *arg1, char *arg2,
					uval wait);
#endif /* #ifndef __KERN_RUN_PROCESS_H_ */
