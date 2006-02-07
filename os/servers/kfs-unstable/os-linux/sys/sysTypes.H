#ifndef __SYS_TYPES_H_
#define __SYS_TYPES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysTypes.H,v 1.2 2004/02/24 20:32:49 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines some basic system types and macroes for
 * accessing them.
 * **************************************************************************/

/* 
   commid is ID used for communication - pid,rd,vp tuple see arch file for bits
   disp is dispatcher id - rd,rv pair see arch file for bits
    ex: on powerPC

            32        32              32      16   16   
        --------------------      ---------------------
commID  |   PID   |  DISP  |  or  |   PID   | RD | VP | 
        --------------------      ---------------------

            16        16
        --------------------
DispID  |   RD    |   VP   |
        --------------------

*/

typedef uval    ProcessID;		// unique number identifies each procss
typedef uval	RDNum;			// resource domain number
typedef uval    VPNum;			// virtual processor number
typedef uval	DispatcherID;		// (RDNum, VPNum) pair - see arch file

typedef uval    CommID;			// ID for communication - see arch file
typedef uval    ThreadID;		// unique number identifies each thread
typedef uval    XHandle;		// Extern handle of an internal ObjRef
typedef uval    TypeID;			// Unique TypeID of a Service/Obj Class
typedef uval64  SysTime;		// time in system dependent units
typedef uval    ObjectHandle;
typedef char    PathName;

// the kernel's own pid is well defined to be zero
#define _KERNEL_PID ((ProcessID)0)
#define _INVALID_PID ((ProcessID)~0ULL)

// SysTime is unsigned, so -1 is as close to infinity as we can get
#define _SYSTIME_MAX ((SysTime) -1)

/*
 * Types that describe code and code addresses for things that are not
 * callable functions.
 */
typedef uval8 code[1];
typedef code  *codeAddress;

//#include __MINC(sysTypes.H)

typedef SysStatus SysStatusProcessID;
#define _SGETPID(ss)     (_SGETUVAL(ss))
#define _SSETPID(ss,val) (_SSETUVAL(ss,val))

typedef SysStatus SysStatusXHandle;
#define _SGETXH(ss)     (_SGETUVAL(ss))
#define _SSETXH(ss,val) (_SSETUVAL(ss,val))

#define _XHANDLE_SEQNO_SHIFT 0
#define _XHANDLE_SEQNO_BITS 8

#define _XHANDLE_IDX_SHIFT 8
#define _XHANDLE_IDX_BITS (8*sizeof(uval) - _XHANDLE_SEQNO_BITS)



extern "C" void __no_builtins_allowed(const char *className, const char *op,
				      const char *file, int line);
#define DEFINE_NOOP_NEW(CLASS)						\
  inline void *operator new(size_t size) {				\
      __no_builtins_allowed(#CLASS, __FUNCTION__, __FILE__, __LINE__);	\
      return (void *)0;							\
  }									\
  inline void * operator new[](size_t size) {				\
      __no_builtins_allowed(#CLASS, __FUNCTION__, __FILE__, __LINE__);	\
      return (void *)0;							\
  }									\
  inline void operator delete(void *p) {                                \
      __no_builtins_allowed(#CLASS, __FUNCTION__, __FILE__, __LINE__);	\
  }									\
  inline void operator delete(void *p, uval size) {			\
      __no_builtins_allowed(#CLASS, __FUNCTION__, __FILE__, __LINE__);	\
  }									\
  inline void operator delete[](void * p, uval size) {			\
      __no_builtins_allowed(#CLASS, __FUNCTION__,  __FILE__, __LINE__);	\
  }

#endif /* #ifndef __SYS_TYPES_H_ */
