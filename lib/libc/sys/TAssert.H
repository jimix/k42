#ifndef __TASSERT_H_
#define __TASSERT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TAssert.H,v 1.31 2005/08/01 01:35:50 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *  err_printf(fmt, ...): output for asserts
 *  console(fmt, ...)   : output to console (processor specific)
 *
 *  X = p ---> never compiled out
 *  X = t ---> not included in nodebug build
 *  MSG... = arguments to err_printf
 *  EX = boolean expression
 *  STMT = set of C statements
 *  Xassert(EX, STMT)      : if EX fails execute STMT and raiseError()
 *  XassertMsg(EX, MSG...) : if EX fails print MSG and raiseError()
 *  XassertSilent(EX, STMT): if EX fails execute STMT (produces no output)
 *  XassertWrn(EX, MSG...) : if EX fails print MSG
 *  raiseError()        : aborts or enters the debugger, or asks the user
 *                        which path to take
 *  errorBehavior()	: controls the behavior of raiseError()
 * **************************************************************************/
#include <sys/SysStatus.H>

enum ErrorSwitch {
    ALWAYS_ABORT,			// normal Unix behaviour, just abort
    ALWAYS_DEBUG,			// always connect debugger
    ALWAYS_ASK				// prompt user for what to do
};


// operations for printing assert information
extern "C" void verr_printf(const char *fmt, va_list ap);
extern "C" sval err_printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

void errorBehavior(ErrorSwitch);	// change behavior of raiseError
void raiseError(void);			// abort or enter debugger

void pre_tassert(const char *failedexpr, const char *fname, uval lineno);
void pre_wassert(const char *failedexpr, const char *fname, uval lineno);

void errorWithMsg(const char *failedexpr, const char *fname, uval lineno,
		  const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

void warnWithMsg(const char *failedexpr, const char *fname, uval lineno,
		 const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

void errorWithRC(SysStatus rc, const char *fname, uval lineno,
		 const char *fmt ...)
    __attribute__ ((format (printf, 4, 5)));

// Doesn't hurt to give compiler hints that we don't expect
// assertions to be true
#ifndef likely
#define likely(x)	__builtin_expect((x),1)
#endif // #ifndef likely
#ifndef unlikely
#define unlikely(x)	__builtin_expect((x),0)
#endif // #ifndef unlikely


// asserts(panics) left in the final code
#define passert(EX,STMT)						\
		if (unlikely(!(EX))) {					\
		     pre_tassert(# EX, __FILE__, __LINE__);		\
		     STMT;						\
		     raiseError();					\
		}

#define passertRC(RC, MSG...)						\
		if (unlikely(_FAILURE(RC))) {				\
		     errorWithRC((RC), __FILE__, __LINE__, MSG);	\
                }

#define passertMsg(EX, MSG...)						\
		if (unlikely(!(EX))) {					\
		     errorWithMsg(# EX, __FILE__, __LINE__, MSG);	\
		}

#define passertWrn(EX, MSG...)						\
		if (unlikely(!(EX))) {					\
		     warnWithMsg(# EX, __FILE__, __LINE__, MSG);	\
		}

#define passertSilent(EX,STMT)						\
		if (unlikely(!(EX))) {					\
		     STMT;						\
		}

#ifndef NDEBUG
#define ASSERTIONS_ON
#endif

/*
 * Comment everything out if no debugging is defined.
 */
#ifdef ASSERTIONS_ON 
#define tassertMsg(EX, MSG...) passertMsg(EX, MSG)
#define tassertRC(RC, MSG...) passertRC(RC, MSG)
#define tassertWrn(EX, MSG...) passertWrn(EX, MSG)
#define tassertSilent(EX, STMT) passertSilent(EX, STMT)
#define tassert(EX,STMT)	passert(EX, STMT)

#else /* #ifndef NDEBUG */
#define tassertMsg(EX, MSG...)
#define tassertRC(RC, MSG...)
#define tassertWrn(EX, MSG...)
#define tassertSilent(EX, STMT)
#define tassert(EX,STMT)
#endif /* #ifndef NDEBUG */

#endif /* #ifndef __TASSERT_H_ */
