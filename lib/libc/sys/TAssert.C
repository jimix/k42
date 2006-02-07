/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TAssert.C,v 1.30 2005/05/25 11:54:59 butrico Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>

ErrorSwitch ErrorFlag = ALWAYS_DEBUG;

void
errorBehavior(ErrorSwitch flag)
{
    ErrorFlag = flag;
}

#include "misc/linkage.H"

void raiseError(void)
{
    if (InDebugger()) {
	err_printf("Hit assertion while in debugger.  Stack:\n");
	enum{callDepth=50};
	uval callChain[callDepth];
	GetCallChainSelf(0, callChain, callDepth);
	for (uval i = 0; (i < callDepth) && (callChain[i] != 0); i++) {
	    err_printf("        %lx\n", callChain[i]);
	}
	err_printf("Continuing ...\n");
	return;
    }

    switch (ErrorFlag) {

    case ALWAYS_ABORT:
	baseAbort();
	break;

    case ALWAYS_DEBUG:
	breakpoint();
	break;
    case ALWAYS_ASK:
#if 0
	err_printf("To do:  Print out program name here.\n");
	err_printf("In raiseError()...\n");
	for (;;) {
	    err_printf("Connect to debugger (c/C) or abort (a/A)? ");
	    // FIXME: get from assert stream
	    sval c = getchar();
	    err_printf("%c\n", (char) c);
	    if (c == 'c' || c == 'C') {
		breakpoint();
		break;
	    } else if (c == 'a' || c == 'A') {
		baseAbort();
		break;
	    }
	}
#endif
	break;

    }
}

void
pre_tassert(const char * /* failedexpr */, const char *fname, uval lineno)
{
    err_printf("ERROR: file \"%s\", line %ld\n", fname, lineno);
}

void
pre_wassert(const char * /* failedexpr */, const char *fname, uval lineno)
{
    err_printf("WARNING: file \"%s\", line %ld\n", fname, lineno);
}

void
errorWithMsg(const char *failedexpr, const char *fname, uval lineno,
	     const char *fmt, ...)
{
    if (InDebugger()) {
	/*
	 * The raiseError() call below will not execute a breakpoint if we're
	 * in the debugger.  But assertions in the debugger are noisy and
	 * confusing, so we ignore them silently.  We may not be able to
	 * continue, but it can't hurt to try.
	 */
	return;
    }

    pre_tassert(failedexpr, fname, lineno);

    va_list ap;
    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);

    raiseError();
}

void
errorWithRC(SysStatus rc, const char *fname, uval lineno,
	    const char *fmt ...)
{
    if (InDebugger()) {
	/*
	 * The raiseError() call below will not execute a breakpoint if we're
	 * in the debugger.  But assertions in the debugger are noisy and
	 * confusing, so we ignore them silently.  We may not be able to
	 * continue, but it can't hurt to try.
	 */
	return;
    }

    err_printf("ERROR: file \"%s\", line %ld, return code ",
	       fname, lineno);
    _SERROR_EPRINT(rc);

    va_list ap;
    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);

    raiseError();
}

void
warnWithMsg(const char *failedexpr, const char *fname, uval lineno,
	    const char *fmt, ...)
{
    pre_wassert(failedexpr, fname, lineno);

    va_list ap;
    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);
}
