/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traptest.c,v 1.1 2004/08/27 20:16:37 rosnbrg Exp $
 *****************************************************************************/
/* Tests traps and trap handlers */

#define _GNU_SOURCE
#include <fenv.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>

void sigfpe(int sig, siginfo_t *info, void *uc_arg)
{
    ucontext_t *uc = (ucontext_t *) uc_arg;
    struct pt_regs *regs = uc->uc_mcontext.regs;
    printf("SIGFPE at pc 0x%lx.\n", regs->nip);
    // we may be past the faulting instruction already
}

void sigtrap(int sig, siginfo_t *info, void *uc_arg)
{
    ucontext_t *uc = (ucontext_t *) uc_arg;
    struct pt_regs *regs = uc->uc_mcontext.regs;
    printf("SIGTRAP at pc 0x%lx.\n", regs->nip);
    regs->nip += 4;	// skip trap instruction
}

void sigsegv(int sig, siginfo_t *info, void *uc_arg)
{
    ucontext_t *uc = (ucontext_t *) uc_arg;
    struct pt_regs *regs = uc->uc_mcontext.regs;
    printf("SIGSEGV at pc 0x%lx.\n", regs->nip);
    regs->nip += 4;	// skip faulting instruction
}

void sigbus(int sig, siginfo_t *info, void *uc_arg)
{
    ucontext_t *uc = (ucontext_t *) uc_arg;
    struct pt_regs *regs = uc->uc_mcontext.regs;
    printf("SIGBUS at pc 0x%lx.\n", regs->nip);
    regs->nip += 4;	// skip faulting instruction
}

void set_handler(int sig, void (*handler)(int, siginfo_t *, void *))
{
    int rcode;
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handler;

    rcode = sigaction(sig, &act, NULL);
    if (rcode < 0) {
	fprintf(stderr, "sigaction returned %d, errno %d\n", rcode, errno);
    }
}

double double_divide(double a, double b)
{
    return a / b;
}

char space[16];

int
main(void) {
    set_handler(SIGFPE, sigfpe);
    set_handler(SIGTRAP, sigtrap);
    set_handler(SIGSEGV, sigsegv);
    set_handler(SIGBUS, sigbus);

    printf("Expecting SIGFPE:\n");
    fesetenv(FE_NOMASK_ENV);
    (void) double_divide(1.0, 0.0);

    printf("Expecting SIGTRAP:\n");
    asm("trap");

    printf("Expecting SIGSEGV:\n");
    char *char_ptr = (char *) 0;
    *char_ptr = 'X';

    printf("Expecting SIGSEGV:\n");
    double *dbl_ptr = (double *) 1;
    (*dbl_ptr) = 1.0;

    // Make sure a misaligned store to a valid address works.
    dbl_ptr = (double *) (((((int) space) + 7) & ~7) + 1);
    (*dbl_ptr) = 1.0;

    printf("Expecting SIGBUS:\n");
    unsigned long long tmp;
    asm("ldarx	%0,0,%1" : "=r" (tmp) : "b" (dbl_ptr));

    return 0;
}
