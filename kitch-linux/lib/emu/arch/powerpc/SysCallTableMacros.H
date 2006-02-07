/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysCallTableMacros.H,v 1.2 2005/08/31 13:56:05 dgurley Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: macros for system call table initialization
 * **************************************************************************/

#include <trace/traceUser.h>

/*
 *syserr_printf is a copy of err_printf WITHOUT
 *the format string check attribute
 *Its too hard to get the paramters to match the format
 *in the generated wrapper routines.
 */
static void syserr_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);
}
                                                                                                                     

/* for each 64 bit syscall the perl script will generate either a SYSCALL_DECLARE
   or SYSCALL_NYS or SYSCALL_NYI */

#define SIGN_EXT(x) uval(sval32(x))

#define CAST_ARG_A(arg) arg
#define CAST_ARG_B(arg) arg
#define CAST_ARG_AS_FMT(arg) (const char*)arg
#define CAST_ARG_BS_FMT(arg)  arg
#define CAST_ARG_ASS_FMT(arg) (const char*)arg
#define CAST_ARG_BSS_FMT(arg) (const char*)arg

#define TRACE_SYSCALL_ENTER(x,sfmt,pid,a,b,c,d,e,f) TraceOSUserSyscallEnter##sfmt(pid,c,d,e,f,CAST_ARG_B##sfmt(b),CAST_ARG_A##sfmt(a),x) 

#define TRACE_SYSCALL_EXIT(x,sfmts,pid,rc) TraceOSUserSyscallExit(pid,rc,x)

/* this does both TRACEPROC and pem tracing */
#define TRACED_PROC_SYSCALL(x, fmt, sfmt, sign, issign, sysc) {					\
        sign;											\
        syserr_printf("0x%lx Syscall: "#x " " fmt, extRegsLocal.dispatcher->processID, a,b,c,d);\
        TRACE_SYSCALL_ENTER(#x,sfmt,extRegsLocal.dispatcher->processID,a,b,c,d,e,f);            \
        uval ret = sysc(a,b,c,d,e,f,stkPtr);							\
        TRACE_SYSCALL_EXIT(#x,sfmt,extRegsLocal.dispatcher->processID,ret);                     \
        syserr_printf(" 0x%lx ret:0x%lx\n", extRegsLocal.dispatcher->processID, ret); 		\
        return ret; 										\
}

/* this does only pem tracing */
#define TRACED_SYSCALL(x, fmt, sfmt, sign, issign, sysc) {					\
        sign;											\
        TRACE_SYSCALL_ENTER(#x,sfmt,extRegsLocal.dispatcher->processID,a,b,c,d,e,f);            \
        uval ret = sysc(a,b,c,d,e,f,stkPtr);							\
        TRACE_SYSCALL_EXIT(#x,sfmt,extRegsLocal.dispatcher->processID,ret);                     \
        return ret; 										\
}

#define SYSCALL_DECLARE(x, fmt, sfmt, sign, issign) \
extern "C" uval __k42_linux_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr); \
extern "C" static uval __trc_k42_linux_##issign##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr); \
static uval __trc_k42_linux_##issign##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr) \
	TRACED_SYSCALL(x, fmt, sfmt, sign, issign, __k42_linux_##x)					\
extern "C" static uval __traced_##issign##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr); \
static uval __traced##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr)                \
        TRACED_PROC_SYSCALL(x, fmt, sfmt, sign, issign, __k42_linux_##x)
 

extern "C" signed long __k42_linux_emulNoSupport (const char *, signed long);
#define SYSCALL_NYS(x, fmt, sfmt, sign, issign, rc) \
extern "C" static uval __NYS##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr);       \
static uval __NYS##issign##_##x(uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr) {\
        return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__,-(rc))); \
}  \
extern "C" static uval __trc_NYS##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr);       \
static uval __trc_NYS##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr)       \
	TRACED_SYSCALL(x, fmt, sfmt, sign, issign, __NYS##issign##_##x)					\
static uval __NYStraced##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr)   \
        TRACED_PROC_SYSCALL(x, fmt, sfmt, sign, issign, __NYS##issign##_##x)

#define SYSCALL_SIGNEXTEND(x, sign) \
extern "C" static uval __trc_signextend_##x( \
uval a, uval b, uval c, uval d, uval e, \
uval f, uval stkPtr); \
static uval __trc_signextend_##x( \
uval a, uval b, uval c, uval d, uval e, \
uval f, uval stkPtr) { \
sign; \
        uval ret = __trc_k42_linux_##x(a,b,c,d,e,f,stkPtr); \
        return ret;}
        
#define SYSCALL_NYI(x, fmt, sign, issign) \
extern "C" static uval __NYI##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr); \
static uval __NYI##issign##_##x( uval a, uval b, uval c, uval d, uval e, uval f, uval stkPtr) { \
        sign; \
        syserr_printf("%ld Syscall NYI: "#x " " fmt "\n", extRegsLocal.dispatcher->processID, a,b,c,d); \
        passert(0,); \
        return uval(-1); \
}
