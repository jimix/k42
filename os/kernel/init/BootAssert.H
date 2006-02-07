#ifndef __BOOT_ASSERT_H_
#define __BOOT_ASSERT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BootAssert.H,v 1.1 2005/03/15 02:35:10 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * Utilities for asserting early in the boot sequence before err_printf
 * is available
 * **************************************************************************/
#include <sys/SysStatus.H>
#include <sys/TAssert.H>

void bootErrorWithMsg(const char *failedexpr, const char *fname, uval lineno,
                  const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));
void bootErrorWithRC(SysStatus rc, const char *fname, uval lineno,
                 const char *fmt ...)
    __attribute__ ((format (printf, 4, 5)));

void
bootErrorWithRC(SysStatus rc, const char *fname, uval lineno,
            const char *fmt ...)
{
    BootPrintf::Printf("ERROR: file \"%s\", line %ld, return code 0x%lx\n",
               fname, lineno, rc);
    va_list ap;
    va_start(ap, fmt);
    BootPrintf::VPrintf(fmt, ap);
    va_end(ap);
    breakpoint();
}

void
bootErrorWithMsg(const char *failedexpr, const char *fname, uval lineno,
             const char *fmt, ...)
{
    err_printf("ERROR: file \"%s\", line %ld\n", fname, lineno);
    va_list ap;
    va_start(ap, fmt);
    BootPrintf::VPrintf(fmt, ap);
    va_end(ap);
    breakpoint();
}

// asserts(panics) left in the final code
#define passertBootRC(RC, MSG...)                                       \
                if (unlikely(_FAILURE(rc))) {                           \
		    bootErrorWithRC(rc, __FILE__, __LINE__, MSG);	\
                }
#define passertBootMsg(EX, MSG...)                                          \
                if (unlikely(!(EX))) {                                  \
                     bootErrorWithMsg(# EX, __FILE__, __LINE__, MSG);       \
                }


#ifndef NDEBUG
#define tassertBootMsg(EX, MSG...) passertBootMsg(EX, MSG)
#define tassertBootRC(RC, MSG...)  passertBootRC(RC, MSG)

#else /* #ifndef NDEBUG */
#define tassertBootMsg(EX, MSG...)
#define tassertBootRC(RC, MSG...)

#endif /* #ifndef NDEBUG */




#endif /* #ifndef __BOOT_ASSERT_H_ */

