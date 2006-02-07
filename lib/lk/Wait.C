/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Wait.C,v 1.2 2004/06/05 19:39:33 mostrows Exp $
 *****************************************************************************/

// Includes code from Linux


#include "lkIncs.H"
#include "LinuxEnv.H"
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>

extern "C" {
#define class __C__class
#include <linux/wait.h>
#undef class
}
#define k42_lock(linuxLock) ((FairBLock*)&(linuxLock)->lock[0])

void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait)
{
	unsigned long flags;

	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	if (wait->func == &default_wake_function
	    || wait->func == &autoremove_wake_function) {
	    tassertMsg(!wait->task || wait->task == current ||
		   wait->task==(struct task_struct *)Scheduler::GetCurThread(),
		   "bad add to wait queue\n");
	    wait->task = (struct task_struct *)Scheduler::GetCurThread();
	}
	__add_wait_queue(q, wait);
	spin_unlock_irqrestore(&q->lock, flags);
}

void add_wait_queue_exclusive(wait_queue_head_t *q, wait_queue_t * wait)
{
	unsigned long flags;

	wait->flags |= WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	if (wait->func == &default_wake_function
	    || wait->func == &autoremove_wake_function) {
	    tassertMsg(!wait->task || wait->task == current ||
		   wait->task==(struct task_struct *)Scheduler::GetCurThread(),
		       "bad add to wait queue\n");
	    wait->task = (struct task_struct *)Scheduler::GetCurThread();
	}
	__add_wait_queue_tail(q, wait);
	spin_unlock_irqrestore(&q->lock, flags);
}

void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
    wait->flags &= ~WQ_FLAG_EXCLUSIVE;
    k42_lock(&q->lock)->acquire();
    if (wait->func == &default_wake_function
	|| wait->func == &autoremove_wake_function) {
	tassertMsg(!wait->task || wait->task == current ||
		   wait->task==(struct task_struct *)Scheduler::GetCurThread(),
		   "bad add to wait queue\n");
	wait->task = (struct task_struct *)Scheduler::GetCurThread();
    }
    if (list_empty(&wait->task_list))
	__add_wait_queue(q, wait);
    k42_lock(&q->lock)->release();
}

void
prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
    wait->flags |= WQ_FLAG_EXCLUSIVE;
    k42_lock(&q->lock)->acquire();
    if (wait->func == &default_wake_function
	|| wait->func == &autoremove_wake_function) {
	tassertMsg(!wait->task || wait->task == current ||
		   wait->task==(struct task_struct *)Scheduler::GetCurThread(),
		   "bad add to wait queue\n");
	wait->task = (struct task_struct *)Scheduler::GetCurThread();
    }
    if (list_empty(&wait->task_list))
	__add_wait_queue(q, wait);
    k42_lock(&q->lock)->release();
}


void
finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
    if (!list_empty(&wait->task_list)) {
	k42_lock(&q->lock)->acquire();
	list_del_init(&wait->task_list);
	k42_lock(&q->lock)->release();
    }
}


int
autoremove_wake_function(wait_queue_t *wait, unsigned mode, int sync)
{
	int ret = default_wake_function(wait, mode, sync);

	if (ret)
		list_del_init(&wait->task_list);
	return ret;
}

int
default_wake_function(wait_queue_t *curr, unsigned mode, int sync)
{
    ThreadID id = ThreadID(curr->task);
    curr->task = NULL;
    Scheduler::Unblock(id);
    return 1;
}




/*
 * The core wakeup function.  Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up.  If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 *
 * There are circumstances in which we can try to wake a task which has already
 * started to run but is not in state TASK_RUNNING.  try_to_wake_up() returns
 * zero in this (rare) case, and we handle it by continuing to scan the queue.
 */
static void __wake_up_common(wait_queue_head_t *q, unsigned int mode, int nr_exclusive, int sync)
{
	struct list_head *tmp, *next;

	list_for_each_safe(tmp, next, &q->task_list) {
		wait_queue_t *curr;
		unsigned flags;
		curr = list_entry(tmp, wait_queue_t, task_list);
		flags = curr->flags;
		if (curr->func(curr, mode, sync) &&
		    (flags & WQ_FLAG_EXCLUSIVE) &&
		    !--nr_exclusive)
			break;
	}
}


//
// Code below is verbatim from kernel/sched.c
//
//

/**
 * __wake_up - wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @mode: which threads
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 */
void __wake_up(wait_queue_head_t *q, unsigned int mode, int nr_exclusive)
{
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__wake_up_common(q, mode, nr_exclusive, 0);
	spin_unlock_irqrestore(&q->lock, flags);
}

/*
 * Same as __wake_up but called with the spinlock in wait_queue_head_t held.
 */
void __wake_up_locked(wait_queue_head_t *q, unsigned int mode)
{
	__wake_up_common(q, mode, 1, 0);
}

/**
 * __wake_up - sync- wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @mode: which threads
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 *
 * The sync wakeup differs that the waker knows that it will schedule
 * away soon, so while the target thread will be woken up, it will not
 * be migrated to another CPU - ie. the two threads are 'synchronized'
 * with each other. This can prevent needless bouncing between CPUs.
 *
 * On UP it can prevent extra preemption.
 */
void __wake_up_sync(wait_queue_head_t *q, unsigned int mode, int nr_exclusive)
{
	unsigned long flags;

	if (unlikely(!q))
		return;

	spin_lock_irqsave(&q->lock, flags);
	if (likely(nr_exclusive))
		__wake_up_common(q, mode, nr_exclusive, 1);
	else
		__wake_up_common(q, mode, nr_exclusive, 0);
	spin_unlock_irqrestore(&q->lock, flags);
}


void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait)
{
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__remove_wait_queue(q, wait);
	spin_unlock_irqrestore(&q->lock, flags);
}


/* wait_on_sync_kiocb:
 *	Waits on the given sync kiocb to complete.
 */
ssize_t wait_on_sync_kiocb(struct kiocb *iocb)
{
	while (iocb->ki_users) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (!iocb->ki_users)
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	return iocb->ki_user_data;
}

extern "C" void complete(struct completion *x);

void complete(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done++;
	__wake_up_common(&x->wait, TASK_UNINTERRUPTIBLE | TASK_INTERRUPTIBLE, 1, 0);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}

void wait_for_completion(struct completion *x)
{
	might_sleep();
	spin_lock_irq(&x->wait.lock);
	if (!x->done) {
		wait_queue_t wait;
		wait.task = (struct task_struct*)Scheduler::GetCurThread();
		wait.func = default_wake_function;
		wait.task_list.next = NULL;
		wait.task_list.prev = NULL;
		wait.flags |= WQ_FLAG_EXCLUSIVE;

		__add_wait_queue_tail(&x->wait, &wait);
		do {
			__set_current_state(TASK_UNINTERRUPTIBLE);
			spin_unlock_irq(&x->wait.lock);
			schedule();
			spin_lock_irq(&x->wait.lock);
		} while (!x->done);
		__remove_wait_queue(&x->wait, &wait);
	}
	x->done--;
	spin_unlock_irq(&x->wait.lock);
}


extern"C" void complete_and_exit(struct completion *comp, long code);
void complete_and_exit(struct completion *comp, long code)
{
	if (comp)
		complete(comp);
	LinuxEnv *le = getLinuxEnv();
	le->destroy();
	Scheduler::DeactivateSelf();
	Scheduler::Exit();
}
