/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SignalUtils.C,v 1.5 2004/08/27 20:16:31 rosnbrg Exp $
 *****************************************************************************/

#include <lk/lkIncs.H>

#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/unistd.h>
#include <asm/sigcontext.h>
#include <asm/ucontext.h>
#include <asm/ppc32.h>

#include "../../SignalUtils.H"
#include <misc/hardware.H>

#define __put_user(val,addr) ((*(addr) = (val)), 0)
#define __get_user(var,addr) (((var) = (*(addr))), 0)
#define put_user(val,addr) __put_user(val,addr)
#define get_user(var,addr) __get_user(var,addr)
#define __copy_to_user(dst,src,size) (memcpy(dst,src,size), 0)
#define __copy_from_user(dst,src,size) (memcpy(dst,src,size), 0)
#define copy_to_user(dst,src,size) __copy_to_user(dst,src,size)
#define copy_from_user(dst,src,size) __copy_from_user(dst,src,size)

// sigaltstack not yet supported
#define on_sig_stack(sp) 0
#define sas_ss_flags(sp) SS_DISABLE

#define flush_icache_range(start,end) CacheSyncRange((start),(end))
#define verify_area(access,addr,size) 0
#define access_ok(access,addr,size) 1

static void
CopyRegsToSigcontext(struct sigcontext *sc,
		     VolatileState *vsp, NonvolatileState *nvsp)
{
    struct pt_regs *pt_regs;

    memset(&sc->gp_regs, 0, sizeof(sc->gp_regs));
    pt_regs = (struct pt_regs *)(&sc->gp_regs);
    memcpy(&(pt_regs->gpr[0]),  &(vsp->r0),   (13 -  0 + 1)*sizeof(vsp->r0));
    memcpy(&(pt_regs->gpr[14]), &(nvsp->r14), (31 - 14 + 1)*sizeof(nvsp->r14));
    pt_regs->nip  = vsp->iar;
    pt_regs->msr  = vsp->msr;
    pt_regs->ctr  = vsp->ctr;
    pt_regs->link = vsp->lr;
    pt_regs->xer  = vsp->xer;
    pt_regs->ccr  = vsp->cr;

    memcpy(&(sc->fp_regs[0]),  &(vsp->f0),   (13 -  0 + 1)*sizeof(vsp->f0));
    memcpy(&(sc->fp_regs[14]), &(nvsp->f14), (31 - 14 + 1)*sizeof(nvsp->f14));
    *((uval64 *)(&sc->fp_regs[32])) = vsp->fpscr;
}

static void
CopyRegsFromSigcontext(struct sigcontext *sc,
		       VolatileState *vsp, NonvolatileState *nvsp)
{
    struct pt_regs *pt_regs;

    pt_regs = (struct pt_regs *)(&sc->gp_regs);
    memcpy(&(vsp->r0),   &(pt_regs->gpr[0]),  (13 -  0 + 1)*sizeof(vsp->r0));
    memcpy(&(nvsp->r14), &(pt_regs->gpr[14]), (31 - 14 + 1)*sizeof(nvsp->r14));
    vsp->iar = pt_regs->nip;
    vsp->msr = pt_regs->msr;
    vsp->ctr = pt_regs->ctr;
    vsp->lr  = pt_regs->link;
    vsp->xer = pt_regs->xer;
    vsp->cr  = pt_regs->ccr;

    memcpy(&(vsp->f0),   &(sc->fp_regs[0]),  (13 -  0 + 1)*sizeof(vsp->f0));
    memcpy(&(nvsp->f14), &(sc->fp_regs[14]), (31 - 14 + 1)*sizeof(nvsp->f14));
    vsp->fpscr = *((uval64 *)(&sc->fp_regs[32]));
}

static void
CopyRegsToMcontext32(struct mcontext32 *mc,
		     VolatileState *vsp, NonvolatileState *nvsp)
{
    struct pt_regs32 *pt_regs;
    uval i;

    memset(&mc->mc_gregs, 0, sizeof(mc->mc_gregs));
    pt_regs = (struct pt_regs32 *)(&mc->mc_gregs);
    for (i = 0; i < 14; i++) {
	pt_regs->gpr[i] = (uval32) (((uval64 *)(&vsp->r0))[i]);
    }
    for (i = 14; i < 32; i++) {
	pt_regs->gpr[i] = (uval32) (((uval64 *)(&nvsp->r14))[i-14]);
    }
    pt_regs->nip  = (uval32) vsp->iar;
    pt_regs->msr  = (uval32) vsp->msr;
    pt_regs->ctr  = (uval32) vsp->ctr;
    pt_regs->link = (uval32) vsp->lr;
    pt_regs->xer  = (uval32) vsp->xer;
    pt_regs->ccr  = (uval32) vsp->cr;

    memcpy(&(mc->mc_fregs[0]),  &(vsp->f0),   (13 -  0 + 1)*sizeof(vsp->f0));
    memcpy(&(mc->mc_fregs[14]), &(nvsp->f14), (31 - 14 + 1)*sizeof(nvsp->f14));
    *((uval64 *)(&mc->mc_fregs[32])) = vsp->fpscr;
}

static void
CopyRegsFromMcontext32(struct mcontext32 *mc,
		       VolatileState *vsp, NonvolatileState *nvsp)
{
    struct pt_regs32 *pt_regs;
    uval i;

    pt_regs = (struct pt_regs32 *)(&mc->mc_gregs);
    for (i = 0; i < 14; i++) {
	(((uval64 *)(&vsp->r0))[i]) = (uval64) pt_regs->gpr[i];
    }
    for (i = 14; i < 32; i++) {
	(((uval64 *)(&nvsp->r14))[i-14]) = (uval64) pt_regs->gpr[i];
    }
    vsp->iar = (uval64) pt_regs->nip;
    vsp->msr = (uval64) pt_regs->msr;
    vsp->ctr = (uval64) pt_regs->ctr;
    vsp->lr  = (uval64) pt_regs->link;
    vsp->xer = (uval64) pt_regs->xer;
    vsp->cr  = (uval64) pt_regs->ccr;

    memcpy(&(vsp->f0),   &(mc->mc_fregs[0]),  (13 -  0 + 1)*sizeof(vsp->f0));
    memcpy(&(nvsp->f14), &(mc->mc_fregs[14]), (31 - 14 + 1)*sizeof(nvsp->f14));
    vsp->fpscr = *((uval64 *)(&mc->mc_fregs[32]));
}

/*
 * **************************************************************************
 * Start of code originally copied from linux/arch/ppc64/kernel/signal.c.
 * **************************************************************************
 */

#define GP_REGS_SIZE	MIN(sizeof(elf_gregset_t), sizeof(struct pt_regs))
#define FP_REGS_SIZE	sizeof(elf_fpregset_t)

#define TRAMP_TRACEBACK	3
#define TRAMP_SIZE	6

/*
 * When we have signals to deliver, we set up on the user stack,
 * going down from the original stack pointer:
 *	1) a rt_sigframe struct which contains the ucontext	
 *	2) a gap of __SIGNAL_FRAMESIZE bytes which acts as a dummy caller
 *	   frame for the signal handler.
 */

struct rt_sigframe {
	/* sys_rt_sigreturn requires the ucontext be the first field */
	struct ucontext uc;
	unsigned long _unused[2];
	unsigned int tramp[TRAMP_SIZE];
	struct siginfo *pinfo;
	void *puc;
	struct siginfo info;
	/* 64 bit ABI allows for 288 bytes below sp before decrementing it. */
	char abigap[288];
};

/*
 * Set up the sigcontext for the signal frame.
 */

static int setup_sigcontext(struct sigcontext *sc,
		 VolatileState *vsp, NonvolatileState *nvsp,
		 int signr, sigset_t *set, unsigned long handler)
{
	/* When CONFIG_ALTIVEC is set, we _always_ setup v_regs even if the
	 * process never used altivec yet (MSR_VEC is zero in pt_regs of
	 * the context). This is very important because we must ensure we
	 * don't lose the VRSAVE content that may have been set prior to
	 * the process doing its first vector operation
	 * Userland shall check AT_HWCAP to know wether it can rely on the
	 * v_regs pointer or not
	 */
#ifdef CONFIG_ALTIVEC
	elf_vrreg_t *v_regs = (elf_vrreg_t *)(((unsigned long)sc->vmx_reserve) & ~0xful);
#endif
	int err = 0;

	//if (regs->msr & MSR_FP)
	//	giveup_fpu(current);

	/* Make sure signal doesn't get spurrious FP exceptions */
	//current->thread.fpscr = 0;
	vsp->fpscr = 0;

#ifdef CONFIG_ALTIVEC
	passertMsg(0, "We don't do ALTIVEC yet.\n");
	//err |= __put_user(v_regs, &sc->v_regs);
	//
	///* save altivec registers */
	//if (current->thread.used_vr) {		
	//	if (regs->msr & MSR_VEC)
	//		giveup_altivec(current);
	//	/* Copy 33 vec registers (vr0..31 and vscr) to the stack */
	//	err |= __copy_to_user(v_regs, current->thread.vr, 33 * sizeof(vector128));
	//	/* set MSR_VEC in the MSR value in the frame to indicate that sc->v_reg)
	//	 * contains valid data.
	//	 */
	//	regs->msr |= MSR_VEC;
	//}
	///* We always copy to/from vrsave, it's 0 if we don't have or don't
	// * use altivec.
	// */
	//err |= __put_user(current->thread.vrsave, (u32 *)&v_regs[33]);
#else /* CONFIG_ALTIVEC */
	err |= __put_user(0, &sc->v_regs);
#endif /* CONFIG_ALTIVEC */
	err |= __put_user((struct pt_regs *)&sc->gp_regs, &sc->regs);
	//err |= __copy_to_user(&sc->gp_regs, regs, GP_REGS_SIZE);
	//err |= __copy_to_user(&sc->fp_regs, &current->thread.fpr, FP_REGS_SIZE);
	CopyRegsToSigcontext(sc, vsp, nvsp);
	err |= __put_user(signr, &sc->signal);
	err |= __put_user(handler, &sc->handler);
	if (set != NULL)
		err |=  __put_user(set->sig[0], &sc->oldmask);

	return err;
}

/*
 * Restore the sigcontext from the signal frame.
 */

static int restore_sigcontext(VolatileState *vsp, NonvolatileState *nvsp,
			      sigset_t *set, int sig, struct sigcontext *sc)
{
#ifdef CONFIG_ALTIVEC
	elf_vrreg_t *v_regs;
#endif
	unsigned int err = 0;
	//unsigned long save_r13;

	///* If this is not a signal return, we preserve the TLS in r13 */
	//if (!sig)
	//	save_r13 = regs->gpr[13];
	//err |= __copy_from_user(regs, &sc->gp_regs, GP_REGS_SIZE);
	//if (!sig)
	//	regs->gpr[13] = save_r13;
	//err |= __copy_from_user(&current->thread.fpr, &sc->fp_regs, FP_REGS_SIZE);
	CopyRegsFromSigcontext(sc, vsp, nvsp);

	if (set != NULL)
		err |=  __get_user(set->sig[0], &sc->oldmask);

#ifdef CONFIG_ALTIVEC
	passertMsg(0, "We don't do ALTIVEC yet.\n");
	//err |= __get_user(v_regs, &sc->v_regs);
	//if (err)
	//	return err;
	///* Copy 33 vec registers (vr0..31 and vscr) from the stack */
	//if (v_regs != 0 && (regs->msr & MSR_VEC) != 0)
	//	err |= __copy_from_user(current->thread.vr, v_regs, 33 * sizeof(vector128));
	//else if (current->thread.used_vr)
	//	memset(&current->thread.vr, 0, 33);
	///* Always get VRSAVE back */
	//if (v_regs != 0)
	//	err |= __get_user(current->thread.vrsave, (u32 *)&v_regs[33]);
	//else
	//	current->thread.vrsave = 0;
#endif /* CONFIG_ALTIVEC */

	///* Force reload of FP/VEC */
	//regs->msr &= ~(MSR_FP | MSR_FE0 | MSR_FE1 | MSR_VEC);

	return err;
}

/*
 * Allocate space for the signal frame
 */
static inline void * get_sigframe(uval sa_flags, VolatileState *vsp,
				  size_t frame_size)
{
        unsigned long newsp;

        /* Default to using normal stack */
        newsp = vsp->r1;

	if (sa_flags & SA_ONSTACK) {
		if (! on_sig_stack(vsp->r1))
			//newsp = (current->sas_ss_sp + current->sas_ss_size);
			passertMsg(0, "SA_ONSTACK not yet implemented.\n");
	}

        return (void *)((newsp - frame_size) & -8ul);
}

/*
 * Setup the trampoline code on the stack
 */
static int setup_trampoline(unsigned int syscall, unsigned int *tramp)
{
	int i, err = 0;

	/* addi r1, r1, __SIGNAL_FRAMESIZE  # Pop the dummy stackframe */
	err |= __put_user(0x38210000UL | (__SIGNAL_FRAMESIZE & 0xffff), &tramp[0]);
	/* li r0, __NR_[rt_]sigreturn| */
	err |= __put_user(0x38000000UL | (syscall & 0xffff), &tramp[1]);
	/* sc */
	err |= __put_user(0x44000002UL, &tramp[2]);

	/* Minimal traceback info */
	for (i=TRAMP_TRACEBACK; i < TRAMP_SIZE ;i++)
		err |= __put_user(0, &tramp[i]);

	if (!err)
		flush_icache_range((unsigned long) &tramp[0],
			   (unsigned long) &tramp[TRAMP_SIZE]);

	return err;
}

int copy_siginfo_to_user(siginfo_t __user *to, siginfo_t *from)
{
	int err;

	if (!access_ok (VERIFY_WRITE, to, sizeof(siginfo_t)))
		return -EFAULT;
	if (from->si_code < 0)
		return __copy_to_user(to, from, sizeof(siginfo_t))
			? -EFAULT : 0;
	/*
	 * If you change siginfo_t structure, please be sure
	 * this code is fixed accordingly.
	 * It should never copy any pad contained in the structure
	 * to avoid security leaks, but must copy the generic
	 * 3 ints plus the relevant union member.
	 */
	err = __put_user(from->si_signo, &to->si_signo);
	err |= __put_user(from->si_errno, &to->si_errno);
	err |= __put_user((short)from->si_code, &to->si_code);
	switch (from->si_code & __SI_MASK) {
	case __SI_KILL:
		err |= __put_user(from->si_pid, &to->si_pid);
		err |= __put_user(from->si_uid, &to->si_uid);
		break;
	case __SI_TIMER:
		 err |= __put_user(from->si_tid, &to->si_tid);
		 err |= __put_user(from->si_overrun, &to->si_overrun);
		 err |= __put_user(from->si_ptr, &to->si_ptr);
		break;
	case __SI_POLL:
		err |= __put_user(from->si_band, &to->si_band);
		err |= __put_user(from->si_fd, &to->si_fd);
		break;
	case __SI_FAULT:
		err |= __put_user(from->si_addr, &to->si_addr);
#ifdef __ARCH_SI_TRAPNO
		err |= __put_user(from->si_trapno, &to->si_trapno);
#endif
		break;
	case __SI_CHLD:
		err |= __put_user(from->si_pid, &to->si_pid);
		err |= __put_user(from->si_uid, &to->si_uid);
		err |= __put_user(from->si_status, &to->si_status);
		err |= __put_user(from->si_utime, &to->si_utime);
		err |= __put_user(from->si_stime, &to->si_stime);
		break;
	case __SI_RT: /* This is not generated by the kernel as of now. */
		err |= __put_user(from->si_pid, &to->si_pid);
		err |= __put_user(from->si_uid, &to->si_uid);
		err |= __put_user(from->si_int, &to->si_int);
		err |= __put_user(from->si_ptr, &to->si_ptr);
		break;
	default: /* this is just in case for now ... */
		err |= __put_user(from->si_pid, &to->si_pid);
		err |= __put_user(from->si_uid, &to->si_uid);
		break;
	}
	return err;
}

/*
 * Do a signal return; undo the signal stack.
 */

void sys_rt_sigreturn(uval stkPtr, VolatileState *vsp, NonvolatileState *nvsp,
		      sigset_t *set)
{
	struct ucontext *uc = (struct ucontext *) stkPtr;

	///* Always make any pending restarted system calls return -EINTR */
	//current_thread_info()->restart_block.fn = do_no_restart_syscall;

	if (verify_area(VERIFY_READ, uc, sizeof(*uc)))
		goto badframe;

	if (__copy_from_user(set, &uc->uc_sigmask, sizeof(*set)))
		goto badframe;
	//restore_sigmask(&set);
	if (restore_sigcontext(vsp, nvsp, NULL, 1, &uc->uc_mcontext))
		goto badframe;

	/* do_sigaltstack expects a __user pointer and won't modify
	 * what's in there anyway
	 */
	//do_sigaltstack(&uc->uc_stack, NULL, regs->gpr[1]);

	return;

badframe:
	passertMsg(0, "badframe in sys_rt_sigreturn, "
		      "vsp=%p nvsp=%p uc=%p &uc->uc_mcontext=%p\n",
		   vsp, nvsp, uc, &uc->uc_mcontext);
	return;
}

static void setup_rt_frame(int signr, __sighandler_t sa_handler, uval sa_flags,
			   siginfo_t *info, sigset_t *set,
			   VolatileState *vsp, NonvolatileState *nvsp)
{
	/* Handler is *really* a pointer to the function descriptor for
	 * the signal routine.  The first entry in the function
	 * descriptor is the entry address of signal and the second
	 * entry is the TOC value we need to use.
	 */
	func_descr_t *funct_desc_ptr;
	struct rt_sigframe *frame;
	unsigned long newsp = 0;
	int err = 0;

	frame = (rt_sigframe *) get_sigframe(sa_flags, vsp, sizeof(*frame));

	if (verify_area(VERIFY_WRITE, frame, sizeof(*frame)))
		goto badframe;

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);
	if (err)
		goto badframe;

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	//err |= __put_user(current->sas_ss_sp, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(vsp->r1),
			  &frame->uc.uc_stack.ss_flags);
	//err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= __put_user(0, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(0, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, vsp, nvsp, signr, NULL,
				(unsigned long)sa_handler);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	if (err)
		goto badframe;

	/* Set up to return from userspace. */
	err |= setup_trampoline(__NR_rt_sigreturn, &frame->tramp[0]);
	if (err)
		goto badframe;

	funct_desc_ptr = (func_descr_t *) sa_handler;

	/* Allocate a dummy caller frame for the signal handler. */
	newsp = (unsigned long)frame - __SIGNAL_FRAMESIZE;
	err |= put_user(0, (unsigned long *)newsp);

	/* Set up "regs" so we "return" to the signal handler. */
	err |= get_user(vsp->iar, &funct_desc_ptr->entry);
	vsp->lr = (unsigned long) &frame->tramp[0];
	vsp->r1 = newsp;
	err |= get_user(vsp->r2, &funct_desc_ptr->toc);
	vsp->r3 = signr;
	if (sa_flags & SA_SIGINFO) {
		err |= get_user(vsp->r4, (unsigned long *)&frame->pinfo);
		err |= get_user(vsp->r5, (unsigned long *)&frame->puc);
		vsp->r6 = (unsigned long) frame;
	} else {
		vsp->r4 = (unsigned long)&frame->uc.uc_mcontext;
	}
	if (err)
		goto badframe;

	return;

badframe:
	passertMsg(0, "badframe in setup_rt_frame, "
		      "vsp=%p nvsp=%p frame=%p newsp=%lx\n",
		   vsp, nvsp, frame, newsp);
	return;
}

/*
 * **************************************************************************
 * End of code originally copied from linux/arch/ppc64/kernel/signal.c.
 * **************************************************************************
 */

/*
 * **************************************************************************
 * Start of code originally copied from linux/arch/ppc64/kernel/signal32.c.
 * **************************************************************************
 */

#define GP_REGS_SIZE32	min(sizeof(elf_gregset_t32), sizeof(struct pt_regs32))

/*
 * When we have signals to deliver, we set up on the
 * user stack, going down from the original stack pointer:
 *	a sigregs32 struct
 *	a sigcontext32 struct
 *	a gap of __SIGNAL_FRAMESIZE32 bytes
 *
 * Each of these things must be a multiple of 16 bytes in size.
 *
 */
struct sigregs32 {
	struct mcontext32	mctx;		/* all the register values */
	/*
	 * Programs using the rs6000/xcoff abi can save up to 19 gp
	 * regs and 18 fp regs below sp before decrementing it.
	 */
	int			abigap[56];
};

/* We use the mc_pad field for the signal return trampoline. */
#define tramp	mc_pad

/*
 *  When we have rt signals to deliver, we set up on the
 *  user stack, going down from the original stack pointer:
 *	one rt_sigframe32 struct (siginfo + ucontext + ABI gap)
 *	a gap of __SIGNAL_FRAMESIZE32+16 bytes
 *  (the +16 is to get the siginfo and ucontext32 in the same
 *  positions as in older kernels).
 *
 *  Each of these things must be a multiple of 16 bytes in size.
 *
 */
struct rt_sigframe32 {
	struct compat_siginfo	info;
	struct ucontext32	uc;
	/*
	 * Programs using the rs6000/xcoff abi can save up to 19 gp
	 * regs and 18 fp regs below sp before decrementing it.
	 */
	int			abigap[56];
};

/*
 * Functions for flipping sigsets (thanks to brain dead generic
 * implementation that makes things simple for little endian only
 */
static inline void compat_from_sigset(compat_sigset_t *compat, sigset_t *set)
{
	switch (_NSIG_WORDS) {
	case 4: compat->sig[5] = set->sig[3] & 0xffffffffull ;
		compat->sig[7] = set->sig[3] >> 32; 
	case 3: compat->sig[4] = set->sig[2] & 0xffffffffull ;
		compat->sig[5] = set->sig[2] >> 32; 
	case 2: compat->sig[2] = set->sig[1] & 0xffffffffull ;
		compat->sig[3] = set->sig[1] >> 32; 
	case 1: compat->sig[0] = set->sig[0] & 0xffffffffull ;
		compat->sig[1] = set->sig[0] >> 32; 
	}
}

static inline void sigset_from_compat(sigset_t *set, compat_sigset_t *compat)
{
	switch (_NSIG_WORDS) {
	case 4: set->sig[3] = compat->sig[6] | (((long)compat->sig[7]) << 32);
	case 3: set->sig[2] = compat->sig[4] | (((long)compat->sig[5]) << 32);
	case 2: set->sig[1] = compat->sig[2] | (((long)compat->sig[3]) << 32);
	case 1: set->sig[0] = compat->sig[0] | (((long)compat->sig[1]) << 32);
	}
}

/*
 * Save the current user registers on the user stack.
 * We only save the altivec registers if the process has used
 * altivec instructions at some point.
 */
static int save_user_regs(VolatileState *vsp, NonvolatileState *nvsp,
			  struct mcontext32 *frame, int sigret)
{
	//elf_greg_t64 *gregs = (elf_greg_t64 *)regs;
	//int i, err = 0;
	
	///* Make sure floating point registers are stored in regs */ 
	//if (regs->msr & MSR_FP)
	//	giveup_fpu(current);
	
	/* save general and floating-point registers */
	//for (i = 0; i <= PT_RESULT; i ++)
	//	err |= __put_user((unsigned int)gregs[i], &frame->mc_gregs[i]);
	//err |= __copy_to_user(&frame->mc_fregs, current->thread.fpr,
	//		      ELF_NFPREG * sizeof(double));
	//if (err)
	//	return 1;
	CopyRegsToMcontext32(frame, vsp, nvsp);

	vsp->fpscr = 0;	/* turn off all fp exceptions */

#ifdef CONFIG_ALTIVEC
	passertMsg(0, "We don't do ALTIVEC yet.\n");
	///* save altivec registers */
	//if (current->thread.used_vr) {
	//	if (regs->msr & MSR_VEC)
	//		giveup_altivec(current);
	//	if (__copy_to_user(&frame->mc_vregs, current->thread.vr,
	//			   ELF_NVRREG32 * sizeof(vector128)))
	//		return 1;
	//	/* set MSR_VEC in the saved MSR value to indicate that
	//	   frame->mc_vregs contains valid data */
	//	if (__put_user(regs->msr | MSR_VEC, &frame->mc_gregs[PT_MSR]))
	//		return 1;
	//}
	///* else assert((regs->msr & MSR_VEC) == 0) */
	//
	///* We always copy to/from vrsave, it's 0 if we don't have or don't
	// * use altivec. Since VSCR only contains 32 bits saved in the least
	// * significant bits of a vector, we "cheat" and stuff VRSAVE in the
	// * most significant bits of that same vector. --BenH
	// */
	//if (__put_user(current->thread.vrsave, (u32 *)&frame->mc_vregs[32]))
	//	return 1;
#endif /* CONFIG_ALTIVEC */

	if (sigret) {
		/* Set up the sigreturn trampoline: li r0,sigret; sc */
		if (__put_user(0x38000000UL + sigret, &frame->tramp[0])
		    || __put_user(0x44000002UL, &frame->tramp[1]))
			return 1;
		flush_icache_range((unsigned long) &frame->tramp[0],
				   (unsigned long) &frame->tramp[2]);
	}

	return 0;
}

/*
 * Restore the current user register values from the user stack,
 * (except for MSR).
 */
static int restore_user_regs(VolatileState *vsp, NonvolatileState *nvsp,
			     struct mcontext32 __user *sr, int sig)
{
	//elf_greg_t64 *gregs = (elf_greg_t64 *)regs;
	//int i, err = 0;
	//unsigned int save_r2;
#ifdef CONFIG_ALTIVEC
	//unsigned long msr;
#endif

	///*
	// * restore general registers but not including MSR. Also take
	// * care of keeping r2 (TLS) intact if not a signal
	// */
	//if (!sig)
	//	save_r2 = (unsigned int)regs->gpr[2];
	//for (i = 0; i < PT_MSR; i ++)
	//	err |= __get_user(gregs[i], &sr->mc_gregs[i]);
	//for (i ++; i <= PT_RESULT; i ++)
	//	err |= __get_user(gregs[i], &sr->mc_gregs[i]);
	//if (!sig)
	//	regs->gpr[2] = (unsigned long) save_r2;
	//if (err)
	//	return 1;
	//
	///* force the process to reload the FP registers from
	//   current->thread when it next does FP instructions */
	//regs->msr &= ~(MSR_FP | MSR_FE0 | MSR_FE1);
	//if (__copy_from_user(current->thread.fpr, &sr->mc_fregs,
	//		     sizeof(sr->mc_fregs)))
	//	return 1;
	CopyRegsFromMcontext32(sr, vsp, nvsp);

#ifdef CONFIG_ALTIVEC
	passertMsg(0, "We don't do ALTIVEC yet.\n");
	///* force the process to reload the altivec registers from
	//   current->thread when it next does altivec instructions */
	//regs->msr &= ~MSR_VEC;
	//if (!__get_user(msr, &sr->mc_gregs[PT_MSR]) && (msr & MSR_VEC) != 0) {
	//	/* restore altivec registers from the stack */
	//	if (__copy_from_user(current->thread.vr, &sr->mc_vregs,
	//			     sizeof(sr->mc_vregs)))
	//		return 1;
	//} else if (current->thread.used_vr)
	//	memset(&current->thread.vr, 0, ELF_NVRREG32 * sizeof(vector128));
	//
	///* Always get VRSAVE back */
	//if (__get_user(current->thread.vrsave, (u32 *)&sr->mc_vregs[32]))
	//	return 1;
#endif /* CONFIG_ALTIVEC */

	return 0;
}

static int copy_siginfo_to_user32(compat_siginfo_t *d, siginfo_t *s)
{
	int err;

	if (!access_ok (VERIFY_WRITE, d, sizeof(*d)))
		return -EFAULT;

	err = __put_user(s->si_signo, &d->si_signo);
	err |= __put_user(s->si_errno, &d->si_errno);
	err |= __put_user((short)s->si_code, &d->si_code);
	if (s->si_signo >= SIGRTMIN) {
		err |= __put_user(s->si_pid, &d->si_pid);
		err |= __put_user(s->si_uid, &d->si_uid);
		err |= __put_user(s->si_int, &d->si_int);
	} else {
		switch (s->si_signo) {
		/* XXX: What about POSIX1.b timers */
		case SIGCHLD:
			err |= __put_user(s->si_pid, &d->si_pid);
			err |= __put_user(s->si_status, &d->si_status);
			err |= __put_user(s->si_utime, &d->si_utime);
			err |= __put_user(s->si_stime, &d->si_stime);
			break;
		case SIGSEGV:
		case SIGBUS:
		case SIGFPE:
		case SIGILL:
			err |= __put_user((long)(s->si_addr), &d->si_addr);
	        break;
		case SIGPOLL:
			err |= __put_user(s->si_band, &d->si_band);
			err |= __put_user(s->si_fd, &d->si_fd);
			break;
		default:
			err |= __put_user(s->si_pid, &d->si_pid);
			err |= __put_user(s->si_uid, &d->si_uid);
			break;
		}
	}
	return err;
}

/*
 * Set up a signal frame for a "real-time" signal handler
 * (one which gets siginfo).
 */
static void handle_rt_signal32(unsigned long sig, __sighandler_t sa_handler,
			       uval sa_flags, siginfo_t *info, sigset_t *oldset,
			       VolatileState *vsp, NonvolatileState *nvsp,
			       unsigned long newsp)
{
	struct rt_sigframe32 __user *rt_sf;
	struct mcontext32 __user *frame = NULL;
	//unsigned long origsp = newsp;
	compat_sigset_t c_oldset;

	/* Set up Signal Frame */
	/* Put a Real Time Context onto stack */
	newsp -= sizeof(*rt_sf);
	rt_sf = (struct rt_sigframe32 __user *)newsp;

	/* create a stack frame for the caller of the handler */
	newsp -= __SIGNAL_FRAMESIZE32 + 16;

	if (verify_area(VERIFY_WRITE, (void __user *)newsp, origsp - newsp))
		goto badframe;

	compat_from_sigset(&c_oldset, oldset);

	/* Put the siginfo & fill in most of the ucontext */
	if (copy_siginfo_to_user32(&rt_sf->info, info)
	    || __put_user(0, &rt_sf->uc.uc_flags)
	    || __put_user(0, &rt_sf->uc.uc_link)
	    //|| __put_user(current->sas_ss_sp, &rt_sf->uc.uc_stack.ss_sp)
	    || __put_user(0, &rt_sf->uc.uc_stack.ss_sp)
	    || __put_user(sas_ss_flags(vsp->r1),
			  &rt_sf->uc.uc_stack.ss_flags)
	    //|| __put_user(current->sas_ss_size, &rt_sf->uc.uc_stack.ss_size)
	    || __put_user(0, &rt_sf->uc.uc_stack.ss_size)
	    || __put_user((u32)(u64)&rt_sf->uc.uc_mcontext, &rt_sf->uc.uc_regs)
	    || __copy_to_user(&rt_sf->uc.uc_sigmask, &c_oldset, sizeof(c_oldset)))
		goto badframe;

	/* Save user registers on the stack */
	frame = &rt_sf->uc.uc_mcontext;
	if (save_user_regs(vsp, nvsp, frame, __NR_rt_sigreturn))
		goto badframe;

	if (put_user(vsp->r1, (unsigned long __user *)newsp))
		goto badframe;
	vsp->r1 = (unsigned long) newsp;
	vsp->r3 = sig;
	vsp->r4 = (unsigned long) &rt_sf->info;
	vsp->r5 = (unsigned long) &rt_sf->uc;
	vsp->r6 = (unsigned long) rt_sf;
	vsp->iar = (unsigned long) sa_handler;
	vsp->lr = (unsigned long) frame->tramp;
	//regs->trap = 0;

	return;

badframe:
	passertMsg(0, "badframe in handle_rt_signal, "
		      "vsp=%p nvsp=%p frame=%p newsp=%lx\n",
		   vsp, nvsp, frame, newsp);
	return;
}

static long do_setcontext32(struct ucontext32 __user *ucp,
			    VolatileState *vsp, NonvolatileState *nvsp,
			    int sig, sigset_t *set)
{
	compat_sigset_t c_set;
	u32 mcp;

	if (__copy_from_user(&c_set, &ucp->uc_sigmask, sizeof(c_set))
	    || __get_user(mcp, &ucp->uc_regs))
		return -EFAULT;
	sigset_from_compat(set, &c_set);
	//restore_sigmask(&set);
	if (restore_user_regs(vsp, nvsp, (struct mcontext32 *)(u64)mcp, sig))
		return -EFAULT;

	return 0;
}

void sys32_rt_sigreturn(uval stkPtr, VolatileState *vsp, NonvolatileState *nvsp,
			sigset_t *set)
{
	struct rt_sigframe32 __user *rt_sf;
	//int ret;


	///* Always make any pending restarted system calls return -EINTR */
	//current_thread_info()->restart_block.fn = do_no_restart_syscall;

	rt_sf = (struct rt_sigframe32 __user *)
		(stkPtr + __SIGNAL_FRAMESIZE32 + 16);
	if (verify_area(VERIFY_READ, rt_sf, sizeof(*rt_sf)))
		goto bad;
	if (do_setcontext32(&rt_sf->uc, vsp, nvsp, 1, set))
		goto bad;

	///*
	// * It's not clear whether or why it is desirable to save the
	// * sigaltstack setting on signal delivery and restore it on
	// * signal return.  But other architectures do this and we have
	// * always done it up until now so it is probably better not to
	// * change it.  -- paulus
	// * We use the sys32_ version that does the 32/64 bits conversion
	// * and takes userland pointer directly. What about error checking ?
	// * nobody does any...
	// */
       	//sys32_sigaltstack((u32)(u64)&rt_sf->uc.uc_stack, 0, 0, 0, 0, 0, regs);

	//regs->result &= 0xFFFFFFFF;
	//ret = regs->result;

	//return ret;
	return;

 bad:
	passertMsg(0, "badframe in sys32_rt_sigreturn, "
		      "vsp=%p nvsp=%p rt_sf=%p\n",
		   vsp, nvsp, rt_sf);
	return;
}

/*
 * OK, we're invoking a handler
 */
static void handle_signal32(unsigned long sig, __sighandler_t sa_handler,
			    uval sa_flags, siginfo_t *info, sigset_t *oldset,
			    VolatileState *vsp, NonvolatileState *nvsp,
			    unsigned long newsp)
{
	struct sigcontext32 __user *sc;
	struct sigregs32 __user *frame;
	//unsigned long origsp = newsp;

	/* Set up Signal Frame */
	newsp -= sizeof(struct sigregs32);
	frame = (struct sigregs32 __user *) newsp;

	/* Put a sigcontext on the stack */
	newsp -= sizeof(*sc);
	sc = (struct sigcontext32 __user *) newsp;

	/* create a stack frame for the caller of the handler */
	newsp -= __SIGNAL_FRAMESIZE32;

	if (verify_area(VERIFY_WRITE, (void *) newsp, origsp - newsp))
		goto badframe;

#if _NSIG != 64
#error "Please adjust handle_signal32()"
#endif
	if (__put_user((u32)(u64)sa_handler, &sc->handler)
	    || __put_user(oldset->sig[0], &sc->oldmask)
	    || __put_user((oldset->sig[0] >> 32), &sc->_unused[3])
	    || __put_user((u32)(u64)frame, &sc->regs)
	    || __put_user(sig, &sc->signal))
		goto badframe;

	if (save_user_regs(vsp, nvsp, &frame->mctx, __NR_sigreturn))
		goto badframe;

	if (put_user(vsp->r1, (unsigned long __user *)newsp))
		goto badframe;
	vsp->r1 = (unsigned long) newsp;
	vsp->r3 = sig;
	vsp->r4 = (unsigned long) sc;
	vsp->iar = (unsigned long) sa_handler;
	vsp->lr = (unsigned long) frame->mctx.tramp;
	//regs->trap = 0;

	return;

badframe:
	passertMsg(0, "badframe in handle_signal, "
		      "vsp=%p nvsp=%p frame=%p newsp=%lx\n",
		   vsp, nvsp, frame, newsp);
	return;
}

/*
 * Do a signal return; undo the signal stack.
 */
void sys32_sigreturn(uval stkPtr, VolatileState *vsp, NonvolatileState *nvsp,
		     sigset_t *set)
{
	struct sigcontext32 __user *sc;
	struct sigcontext32 sigctx;
	struct mcontext32 __user *sr;
	//int ret;

	///* Always make any pending restarted system calls return -EINTR */
	//current_thread_info()->restart_block.fn = do_no_restart_syscall;

	sc = (struct sigcontext32 __user *)(stkPtr + __SIGNAL_FRAMESIZE32);
	if (copy_from_user(&sigctx, sc, sizeof(sigctx)))
		goto badframe;

	/*
	 * Note that PPC32 puts the upper 32 bits of the sigmask in the
	 * unused part of the signal stackframe
	 */
	set->sig[0] = sigctx.oldmask + ((long)(sigctx._unused[3]) << 32);
	//restore_sigmask(&set);

	sr = (struct mcontext32 *)(u64)sigctx.regs;
	if (verify_area(VERIFY_READ, sr, sizeof(*sr))
	    || restore_user_regs(vsp, nvsp, sr, 1))
		goto badframe;

	//regs->result &= 0xFFFFFFFF;
	//ret = regs->result;
	//return ret;
	return;

badframe:
	passertMsg(0, "badframe in sys32_sigreturn, "
		      "vsp=%p nvsp=%p sc=%p sr=%p\n",
		   vsp, nvsp, sc, sr);
	return;
}

/*
 * **************************************************************************
 * End of code originally copied from linux/arch/ppc64/kernel/signal32.c.
 * **************************************************************************
 */

/*
 * **************************************************************************
 * Start of code originally copied from linux/arch/ppc64/kernel/traps.c.
 * **************************************************************************
 */

static void parse_fpe(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		      VolatileState *vsp, NonvolatileState *nvsp,
		      siginfo_t *info)
{
	//siginfo_t info;
	unsigned long fpscr;

	//if (regs->msr & MSR_FP)
	//	giveup_fpu(current);

	fpscr = vsp->fpscr;

	/* Invalid operation */
	if ((fpscr & FPSCR_VE) && (fpscr & FPSCR_VX))
		info->si_code = FPE_FLTINV;

	/* Overflow */
	else if ((fpscr & FPSCR_OE) && (fpscr & FPSCR_OX))
		info->si_code = FPE_FLTOVF;

	/* Underflow */
	else if ((fpscr & FPSCR_UE) && (fpscr & FPSCR_UX))
		info->si_code = FPE_FLTUND;

	/* Divide by zero */
	else if ((fpscr & FPSCR_ZE) && (fpscr & FPSCR_ZX))
		info->si_code = FPE_FLTDIV;

	/* Inexact result */
	else if ((fpscr & FPSCR_XE) && (fpscr & FPSCR_XX))
		info->si_code = FPE_FLTRES;

	else
		info->si_code = 0;

	info->si_signo = SIGFPE;
	info->si_errno = 0;
	info->si_addr = (void *)vsp->iar;
	//_exception(SIGFPE, &info, regs);
}

void
ProgramCheckException(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		      VolatileState *vsp, NonvolatileState *nvsp,
		      siginfo_t *info)
{
	//siginfo_t info;

	if (vsp->msr & 0x100000) {
		/* IEEE FP exception */

		parse_fpe(trapNumber, trapInfo, trapAuxInfo,
		          vsp, nvsp, info);
	} else if (vsp->msr & 0x40000) {
		/* Privileged instruction */

		info->si_signo = SIGILL;
		info->si_errno = 0;
		info->si_code = ILL_PRVOPC;
		info->si_addr = (void *)vsp->iar;
		//_exception(SIGILL, &info, regs);
	} else if (vsp->msr & 0x20000) {
		/* trap exception */

		//if (debugger_bpt(regs))
		//	return;

		//if (check_bug_trap(regs)) {
		//	regs->nip += 4;
		//	return;
		//}
		info->si_signo = SIGTRAP;
		info->si_errno = 0;
		info->si_code = TRAP_BRKPT;
		info->si_addr = (void *)vsp->iar;
		//_exception(SIGTRAP, &info, regs);
	} else {
		/* Illegal instruction */

		info->si_signo = SIGILL;
		info->si_errno = 0;
		info->si_code = ILL_ILLTRP;
		info->si_addr = (void *)vsp->iar;
		//_exception(SIGILL, &info, regs);
	}
}

static void
AlignmentException(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		   VolatileState *vsp, NonvolatileState *nvsp,
		   siginfo_t *info)
{
	//int fixed;
	//siginfo_t info;

	/*
	 * Alignment exceptions are fixed (if possible) before we get here.
	 */

	//fixed = fix_alignment(regs);

	//if (fixed == 1) {
	//	if (!user_mode(regs))
	//		PPCDBG(PPCDBG_ALIGNFIXUP, "fix alignment at %lx\n",
	//		       regs->nip);
	//	vsp->iar += 4;	/* skip over emulated instruction */
	//	return;
	//}

	/* Operand address was bad */	
	//if (fixed == -EFAULT) {
	//	if (user_mode(regs)) {
	//		info->si_signo = SIGSEGV;
	//		info->si_errno = 0;
	//		info->si_code = SEGV_MAPERR;
	//		info->si_addr = (void *)trapAuxInfo;
	//		force_sig_info(SIGSEGV, &info, current);
	//	} else {
	//		/* Search exception table */
	//		bad_page_fault(regs, regs->dar, SIGSEGV);
	//	}

	//	return;
	//}

	info->si_signo = SIGBUS;
	info->si_errno = 0;
	info->si_code = BUS_ADRALN;
	info->si_addr = (void *)vsp->iar;
	//_exception(SIGBUS, &info, regs);
}

/*
 * **************************************************************************
 * End of code originally copied from linux/arch/ppc64/kernel/traps.c.
 * **************************************************************************
 */

/*static*/ void
SignalUtils::SignalReturn(SignalReturnType srType, uval stkPtr,
			  VolatileState *vsp, NonvolatileState *nvsp,
			  uval64& oldmask)
{
    sigset_t oldsigmask;

    switch (srType) {
    case SIGRETURN_64:
    case SIGRETURN_RT_64:
	sys_rt_sigreturn(stkPtr, vsp, nvsp, &oldsigmask);
	break;
    case SIGRETURN_32:
	sys32_sigreturn(stkPtr, vsp, nvsp, &oldsigmask);
	break;
    case SIGRETURN_RT_32:
	sys32_rt_sigreturn(stkPtr, vsp, nvsp, &oldsigmask);
	break;
    default:
	passertMsg(0, "Unexpected sigreturn type %d.\n", srType);
    }

    oldmask = oldsigmask.sig[0];
}

/*static*/ void
SignalUtils::PushSignal(VolatileState *vsp, NonvolatileState *nvsp,
			__sighandler_t handler, uval flags, sval sig,
			uval64 oldmask)
{
    /*
     * We aren't doing siginfo yet.  Create a zeroed dummy.
     */
    siginfo_t info;
    info.si_signo = sig;
    info.si_errno = 0;
    info.si_code = 0;
    info.si_pid = 0;
    info.si_uid = 0;

    sigset_t oldsigmask;
    oldsigmask.sig[0] = oldmask;

    if ((vsp->msr & PSL_SF) != 0) {
	// 64-bit sandbox
	setup_rt_frame(sig, handler, flags, &info, &oldsigmask, vsp, nvsp);
    } else {
	// 32-bit sandbox
        /* Default to using normal stack */
        unsigned long newsp;
        newsp = vsp->r1;
	if (flags & SA_ONSTACK) {
		if (! on_sig_stack(vsp->r1))
			//newsp = (current->sas_ss_sp + current->sas_ss_size);
			passertMsg(0, "SA_ONSTACK not yet implemented.\n");
	}
	newsp &= ~0xfUL;

	if (flags & SA_SIGINFO) {
	    handle_rt_signal32(sig, handler, flags, &info, &oldsigmask,
			       vsp, nvsp, newsp);
	} else {
	    handle_signal32(sig, handler, flags, &info, &oldsigmask,
			    vsp, nvsp, newsp);
	}
    }
}

static void
BadAddressException(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		    VolatileState *vsp, NonvolatileState *nvsp,
		    siginfo_t *info)
{
	info->si_signo = SIGSEGV;
	info->si_errno = 0;
	info->si_code = ((trapInfo & DSIStatusReg::protect) != 0) ?
						SEGV_ACCERR : SEGV_MAPERR;
	info->si_addr = (void *) trapAuxInfo;
}

/*static*/ void
SignalUtils::ConvertTrapToSignal(uval trapNumber, uval trapInfo,
				 uval trapAuxInfo,
				 VolatileState *vsp, NonvolatileState *nvsp,
				 sval &sig)
{
    siginfo_t info;
    info.si_signo = 0;

    switch (trapNumber) {
    case EXC_DSI:
    case EXC_ISI:
	BadAddressException(trapNumber, trapInfo, trapAuxInfo,
			    vsp, nvsp, &info);
	break;
    case EXC_ALI:
	AlignmentException(trapNumber, trapInfo, trapAuxInfo,
			   vsp, nvsp, &info);
	break;
    case EXC_PGM:
	ProgramCheckException(trapNumber, trapInfo, trapAuxInfo,
			      vsp, nvsp, &info);
	break;
    default:
	passertMsg(0, "Unexpected trapNumber 0x%lx.\n", trapNumber);
	break;
    }

    sig = info.si_signo;
}
