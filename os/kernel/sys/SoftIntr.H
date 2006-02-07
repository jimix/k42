#ifndef __SOFT_INTR_H_
#define __SOFT_INTR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SoftIntr.H,v 1.31 2003/09/18 13:21:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The various software level interrupts.  These
 * are sent either by the kernel or by an explicit request on the
 * process by the user himself.
 * **************************************************************************/

class SoftIntr {
    friend void genConstants(void);
    friend class ExceptionLocal;
    volatile uval32 flags;
public:
#include <defines/debug_soft_intrs.H>
#ifdef DEBUG_SOFT_INTERRUPTS
    volatile sval outstanding[32];
#endif /* #ifdef DEBUG_SOFT_INTERRUPTS */

    enum {
    };

    enum IntrType {
	// interrupts to kernel
	LINUX_SOFTINTR    = 1,		// FIXME: make opaque

	// interrupts from kernel to scheduler
	PREEMPT           = 2,
	PRINT_STATUS      = 3,		// for debugging

	// tell the recipient to check the queue for:
	MP_DISABLED_SEND  = 4,		// disabled mp message sends
	MP_DISABLED_REPLY = 5,		// disabled mp message replies
	MP_ENABLED_SEND   = 6,		// enabled mp message sends
	MP_ENABLED_REPLY  = 7,		// enabled mp message replies
	ASYNC_MSG         = 8,		// async messages
	PGFLT_COMPLETION  = 9,		// page-fault completion notifications
	TIMER_EVENT       = 10,		// dispatcher timer interrupt
	IPC_RETRY_NOTIFY  = 11,		// IPC retry notifications
	PULSE             = 12,		// heartbeat for thread time-slicing

#ifdef TARGET_mips64
	// interrupts to kernel
	GIZMO_SOFTINTR    = 30,		// FIXME: make opaque
#endif /* #ifdef TARGET_mips64 */

	MAX_INTERRUPTS    = 32		// maximum number of interrupts
    };

    typedef void (*IntrFunc)(IntrType);

    SoftIntr fetchAndClear() {
	SoftIntr tmp;
	tmp.flags = Swap32Volatile(&flags, (uval32)0);
	return tmp;
    }

    SoftIntr fetchAndSet(IntrType v) {
	SoftIntr tmp;
	tmp.flags = FetchAndOr32Volatile(&flags, (0x1<<v));
#ifdef DEBUG_SOFT_INTERRUPTS
	if ((tmp.flags & 0x1<<v) == 0) {
	    (void) FetchAndAddSigned(&outstanding[v], 1);
	}
#endif /* #ifdef DEBUG_SOFT_INTERRUPTS */
	return tmp;
    }

#ifdef DEBUG_SOFT_INTERRUPTS
    void init() {
	flags = 0;
	for (uval i = 0; i < MAX_INTERRUPTS; i++) outstanding[i] = 0;
    }
#else /* #ifdef DEBUG_SOFT_INTERRUPTS */
    void init()			{ flags = 0; }
#endif /* #ifdef DEBUG_SOFT_INTERRUPTS */

    void set(IntrType v)	{ AtomicOr32Volatile(&flags, (0x1<<v)); }

    void clear(IntrType v)	{ AtomicAnd32Volatile(&flags, ~(0x1<<v)); }

    uval pending()		{ return flags; }	// pseudo-boolean

    uval pending(IntrType v)	{ return ((flags & (0x1<<v)) != 0); }

    /*
     * This routine is not atomic.  It can only be used on a private copy
     * of an interrupt vector.
     */
    IntrType getAndClearFirstPending() {
	uval i;
	uval32 bits;
	i = 0;
	bits = flags;
	tassertMsg(bits != 0, "No bits set.\n");
	if ((bits & 0xffff) == 0) {bits >>= 16; i += 16;}
	if ((bits &   0xff) == 0) {bits >>=  8; i +=  8;}
	if ((bits &    0xf) == 0) {bits >>=  4; i +=  4;}
	if ((bits &    0x3) == 0) {bits >>=  2; i +=  2;}
	if ((bits &    0x1) == 0) {bits >>=  1; i +=  1;}
	flags = (bits ^ 0x1) << i;	// clear the bit we found
	return IntrType(i);
    }
};

#endif /* #ifndef __SOFT_INTR_H_ */
