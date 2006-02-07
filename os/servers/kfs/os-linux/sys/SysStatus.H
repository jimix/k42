#ifndef __SYS_STATUS_H_
#define __SYS_STATUS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysStatus.H,v 1.2 2003/03/10 15:35:21 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic Kitchawan status/error type, Errors are
 * divided into three parts, 1) ErrorCode: a unique 15 bit number used
 * for reporting specific errors to os developers (for classes, this
 * is a number that uniquely identifies the class which generated the
 * error), 2) ClassCode: a code specific to the class invoked, and
 * possibly interpreted by caller, and 3) GenCode: a generic error
 * code which can be set to any of the errors in sys/errno.h
 * Unique ErrorCode numbers are generated using the tool nextErrNumber.
 *
 * If a call is made on a deleted object, the reserved ClassCode of 0xFF
 * is returned.
 * **************************************************************************/

typedef sval SysStatus ;
#define _SYSSTAT_FLAG (((sval64)(1))<<((sizeof(SysStatus)*8)-1))

#ifndef KFS_TOOLS
// constructing a sys status
#define _SERROR(ec, cc, gc)  (_SYSSTAT_FLAG|(ec)<<16|(cc)<<8|(gc))
// WARNING:  There are assembler versions of this macro in some of the
//           machine-dependent asm.h files.  They must be kept consistent.
#endif

// testing for succes
#define _SUCCESS(ss)         ((ss)>=0)
#define _FAILURE(ss)         ((ss)<0)
#define _IF_FAILURE_RET(ss)  if ((ss)<0) return (ss);
#define _IF_FAILURE_RET_VERBOSE(ss) if ((ss)<0) {			\
	tassertWrn(0, "Failure: %s code: %016lx\n", __func__, (ss));	\
	return (ss);							\
}


// operations to extract error codes
#define _SERRCD(i)           (((i)>>16)&0xFFFFFFUL)
#define _SCLSCD(i)           (((i)>>8)&0xFFUL)
#define _SGENCD(i)           (((i)&0xFFUL))
// N.B. this macro assumes that a failure has been detected
//      it does NOT test the failure bit again
#define _SDELETED(ec)        (_SERROR(ec, 0xFF, 0))
#define _ISDELETED(ss)       (_SCLSCD(ss) == 0xFF)

/* bunch of logically derived classes */
typedef SysStatus SysStatusUval;
#define _SGETUVAL(ss) ((uval)ss)
#define _SRETUVAL(val) ((SysStatus)(val))
#define _SSETUVAL(ss,val) (ss=(SysStatus)(val))

/* this should go in some file system specific place, here as an example */
typedef SysStatus SysStatusFD;
#define _SGETFD(ss) (_SGETUVAL(ss))
#define _SSETFD(ss,val) (_SSETUVAL(ss,val))

#endif /* #ifndef __SYS_STATUS_H_ */
