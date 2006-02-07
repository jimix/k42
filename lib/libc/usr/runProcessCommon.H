#ifndef __RUN_PROCESS_COMMON_H_
#define __RUN_PROCESS_COMMON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: runProcessCommon.H,v 1.32 2004/04/28 23:04:38 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines routines exported by:
 *                     userRunProcess.C and kernRunProcess.C
 ****************************************************************************/
#include <usr/ProgExec.H>

SysStatusProcessID
RunProcessCommon(const char *name, uval imageStart, ObjectHandle imageFROH,
		 ProgExec::ArgDesc *args, char const *cwd,
		 ProcessID myPID, uval wait);

void
RunProcessClose(ProgExec::BinInfo &exec);

SysStatus
shellInterp(char *interp, uval size, ProgExec::ArgDesc* argDesc);

SysStatus
PutStrings(uval vecCount, char **childVec, uval &index, uval childBuf,
	   uval buf, uval bufSize, char *const inVec[]);

SysStatus
PutVector(uval &vecCount, char **&childVec, uval &index, uval childBuf,
	  uval buf, uval bufSize, char *const inVec[]);


/*
 * following is provided by seperate implementations in kernel and user.
 * kernel strips path, uses only base name
 * kernel uses bootservers logic, user mode file system
 */
SysStatus
RunProcessOpen(const char *name, uval *imageStart, ObjectHandle *imageFROH,
	       FileLinuxRef *fileRef);
#endif /* #ifndef __RUN_PROCESS_COMMON_H_ */
