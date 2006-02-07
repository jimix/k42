#ifndef __FAIR_BLOCK_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __FAIR_BLOCK_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FairBLock.H,v 1.12 2002/03/21 12:55:09 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of FairBLock.H
 * **************************************************************************/

inline uval
__FairLockAcquire(uval lockAddr, uval lockTailAddr)
{
    uval prev;				// previous value
    __asm__ __volatile__ ("\n"
	"	# FairBLock acquire					\n"
	"	ldarx	%0,0,%2		# prev = lock.tail  [linked]	\n"
	"	cmpldi	%0,0		# if (prev != 0)		\n"
	"	bne-	$+16		#     goto end			\n"
	"	stdcx.	%1,0,%2	# lock.tail = &lock [conditional]	\n"
	"	bne-	$-16		# if (store failed) retry	\n"
	"        isync                  # success, make sure no 	\n"
	"				# instructions get past		\n"
	"	# end FairBLock acquire					\n"
	: "=&r" (prev)
	: "r" (lockAddr), "r" (lockTailAddr)
	);
    return prev;
}

inline uval
__FairLockRelease(uval lockAddr, uval lockTailAddr)
{
    uval prev;
    uval nval = 0;
    __asm__ __volatile__ ("\n"
	"	# FairBLock::release					\n"
	"	sync		# force all modificatoins to complete	\n"
	"	ldarx	%0,0,%3		# prev = lock.tail[linked]	\n"
	"	cmpld	%0,%1		# if (prev != &lock)		\n"
	"	bne-	$+12		#     goto failure		\n"
	"	stdcx.	%2,0,%3	# lock.tail = nval i.e. 0 [conditional]	\n"
	"	bne-	$-16		# if (store failed) retry	\n"
	"	# end FairBLock::release				\n"
	: "=&r" (prev)
	: "r" (lockAddr), "r" (nval), "r" (lockTailAddr)
	);
    return prev;
}

inline void
FairBLock::acquire()
{
    uval prev = __FairLockAcquire((uval)(&lock), (uval)(&lock.tail));
    if (prev!=0) _acquire();
}

inline void
FairBLock::release()
{
    uval prev = __FairLockRelease((uval)(&lock), (uval)(&lock.tail));
    if (prev!=(uval)&lock) _release();
}

inline uval
FairBLock::tryAcquire(void)
{
    return CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock));
}

