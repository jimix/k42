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
 * $Id: sysTypes.H,v 1.38 2003/09/30 11:46:12 rosnbrg Exp $
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

#include __MINC(sysTypes.H)

typedef SysStatus SysStatusProcessID;
#define _SGETPID(ss)     (_SGETUVAL(ss))
#define _SRETPID(val)    (_SRETUVAL(val))
#define _SSETPID(ss,val) (_SSETUVAL(ss,val))

typedef SysStatus SysStatusXHandle;
#define _SGETXH(ss)     (_SGETUVAL(ss))
#define _SSETXH(ss,val) (_SSETUVAL(ss,val))

/*
 * Typedef for a generic function that takes a uval argument and returns
 * a SysStatus.
 */
typedef SysStatus (*SysStatusFunctionUval)(uval);

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
  inline void operator delete(void *p, uval size) {			\
      __no_builtins_allowed(#CLASS, __FUNCTION__, __FILE__, __LINE__);	\
  }									\
  inline void operator delete[](void * p, uval size) {			\
      __no_builtins_allowed(#CLASS, __FUNCTION__,  __FILE__, __LINE__);	\
  }

class SysTypes : public SysTypesArch {
public:
    static uval const PID_MASK   = (uval(1) << PID_BITS)   - 1;
    static uval const DSPID_MASK = (uval(1) << DSPID_BITS) - 1;
    static uval const RD_MASK    = (uval(1) << RD_BITS)    - 1;
    static uval const VP_MASK    = (uval(1) << VP_BITS)    - 1;

    static uval const COMMID_RD_SHIFT = COMMID_DSPID_SHIFT + DSPID_RD_SHIFT;
    static uval const COMMID_VP_SHIFT = COMMID_DSPID_SHIFT + DSPID_VP_SHIFT;

    static VPNum const VP_WILD = VPNum(VP_MASK);
    static CommID const COMMID_NULL = CommID(-1);

    static inline DispatcherID DSPID(RDNum rd, VPNum vp) {
        DispatcherID retvalue;
	retvalue = DispatcherID((uval(rd) << DSPID_RD_SHIFT) |
			    (uval(vp) << DSPID_VP_SHIFT));
	return(retvalue);
    }

    static inline CommID COMMID(ProcessID pid, DispatcherID dspid) {
        CommID retvalue;
	retvalue = CommID((uval(pid) << COMMID_PID_SHIFT) |
		      (uval(dspid) << COMMID_DSPID_SHIFT));
	return(retvalue);
    }

    static inline CommID WILD_COMMID(CommID commid) {
	return commid | (VP_WILD << COMMID_VP_SHIFT);
    }

    static inline RDNum RD_FROM_DSPID(DispatcherID dspid) {
	return RDNum((dspid >> DSPID_RD_SHIFT) & RD_MASK);
    }

    static inline VPNum VP_FROM_DSPID(DispatcherID dspid) {
	return VPNum((dspid >> DSPID_VP_SHIFT) & VP_MASK);
    }

    static inline void UNPACK_DSPID(DispatcherID dspid, RDNum &rd, VPNum &vp) {
	rd = RD_FROM_DSPID(dspid);
	vp = VP_FROM_DSPID(dspid);
    }

    static inline uval DSPID_VP_COMPARE(DispatcherID d1, DispatcherID d2) {
	return VP_FROM_DSPID(d1 ^ d2);
    }

    static inline ProcessID PID_FROM_COMMID(CommID commid) {
	return ProcessID((commid >> COMMID_PID_SHIFT) & PID_MASK);
    }

    static inline DispatcherID DSPID_FROM_COMMID(CommID commid) {
	return DispatcherID((commid >> COMMID_DSPID_SHIFT) & DSPID_MASK);
    }

    static inline RDNum RD_FROM_COMMID(CommID commid) {
	return RDNum(((commid) >> COMMID_RD_SHIFT) & RD_MASK);
    }

    static inline VPNum VP_FROM_COMMID(CommID commid) {
	return VPNum(((commid) >> COMMID_VP_SHIFT) & VP_MASK);
    }

    static inline uval COMMID_PID_MATCH(CommID cid1, CommID cid2) {
	return PID_FROM_COMMID(cid1) == PID_FROM_COMMID(cid2);
    }

    static CommID const KERNEL_COMMID = (_KERNEL_PID << COMMID_PID_SHIFT) |
					(0           << COMMID_RD_SHIFT) |
					(VP_WILD     << COMMID_VP_SHIFT);
};

#endif /* #ifndef __SYS_TYPES_H_ */
