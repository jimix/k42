/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: workqueue.C,v 1.2 2004/06/05 19:39:34 mostrows Exp $
 *****************************************************************************/

/*
 * linux/kernel/workqueue.c
 *
 * Generic mechanism for defining kernel helper threads for running
 * arbitrary tasks in process context.
 *
 * Started by Ingo Molnar, Copyright (C) 2002
 *
 * Derived from the taskqueue/keventd code by:
 *
 *   David Woodhouse <dwmw2@redhat.com>
 *   Andrew Morton <andrewm@uow.edu.au>
 *   Kai Petzke <wpp@marie.physik.tu-berlin.de>
 *   Theodore Ts'o <tytso@mit.edu>
 */

#define __KERNEL_SYSCALLS__

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
extern "C" {
#define private __C__private
#define typename __C__typename
#define virtual __C__virtual
#define new __C__new
#define class __C__class
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <linux/signal.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#undef private
#undef typename
#undef new
#undef class
#undef virtual
}

static void k42_worker_thread(uval data);

/*
 * The per-CPU workqueue.
 *
 * The sequence counters are for flush_scheduled_work().  It wants to wait
 * until until all currently-scheduled works are completed, but it doesn't
 * want to be livelocked by new, incoming ones.  So it waits until
 * remove_sequence is >= the insert_sequence which pertained when
 * flush_scheduled_work() was called.
 */
struct cpu_workqueue_struct {

	spinlock_t lock;

	long remove_sequence;	/* Least-recently added (next to run) */
	long insert_sequence;	/* Next to add */

	struct list_head worklist;
	wait_queue_head_t work_done;

	struct workqueue_struct *wq;
	task_t *thread;

} ____cacheline_aligned;

/*
 * The externally visible workqueue abstraction is an array of
 * per-CPU workqueues:
 */
struct workqueue_struct {
	struct cpu_workqueue_struct cpu_wq[NR_CPUS];
};

/*
 * Queue work on a workqueue. Return non-zero if it was successfully
 * added.
 *
 * We queue the work to the CPU it was submitted
 */
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	unsigned long flags;
	int ret = 0, cpu = get_cpu();
	struct cpu_workqueue_struct *cwq = wq->cpu_wq + cpu;

	if (!test_and_set_bit(0, &work->pending)) {
		BUG_ON(!list_empty(&work->entry));
		work->wq_data = cwq;

		spin_lock_irqsave(&cwq->lock, flags);
		list_add_tail(&work->entry, &cwq->worklist);
		cwq->insert_sequence++;

		// K42 prefers to start a new thread instead
		// wake_up(&cwq->more_work);
		if (CompareAndStore64((uval64*)&cwq->thread, 0ULL,1)) {
			Scheduler::ScheduleFunction(k42_worker_thread,
						    (uval)cwq);
		}
		spin_unlock_irqrestore(&cwq->lock, flags);
		ret = 1;
	}
	put_cpu();
	return ret;
}

static void delayed_work_timer_fn(unsigned long __data)
{
	struct work_struct *work = (struct work_struct *)__data;
	struct cpu_workqueue_struct *cwq =
	    (struct cpu_workqueue_struct*)work->wq_data;
	unsigned long flags;

	/*
	 * Do the wakeup within the spinlock, so that flushing
	 * can be done in a guaranteed way.
	 */
	spin_lock_irqsave(&cwq->lock, flags);
	list_add_tail(&work->entry, &cwq->worklist);
	cwq->insert_sequence++;

	// K42 prefers to start a new thread instead
	// wake_up(&cwq->more_work);
	if (CompareAndStore64((uval64*)&cwq->thread, 0ULL,1)) {
		Scheduler::ScheduleFunction(k42_worker_thread,
					    (uval)cwq);
	}

	spin_unlock_irqrestore(&cwq->lock, flags);
}

int queue_delayed_work(struct workqueue_struct *wq,
			struct work_struct *work, unsigned long delay)
{
	int ret = 0, cpu = get_cpu();
	struct timer_list *timer = &work->timer;
	struct cpu_workqueue_struct *cwq = wq->cpu_wq + cpu;

	if (!test_and_set_bit(0, &work->pending)) {
		BUG_ON(timer_pending(timer));
		BUG_ON(!list_empty(&work->entry));

		work->wq_data = cwq;
		timer->expires = jiffies + delay;
		timer->data = (unsigned long)work;
		timer->function = delayed_work_timer_fn;
		add_timer(timer);
		ret = 1;
	}
	put_cpu();
	return ret;
}

static inline void run_workqueue(struct cpu_workqueue_struct *cwq)
{
	unsigned long flags;

	/*
	 * Keep taking off work from the queue until
	 * done.
	 */
	tassertMsg(preempt_count()==0,"Bad state\n");
	spin_lock_irqsave(&cwq->lock, flags);
	while (!list_empty(&cwq->worklist)) {
		struct work_struct *work = list_entry(cwq->worklist.next,
						struct work_struct, entry);
		void (*f) (void *) = work->func;
		void *data = work->data;

		list_del_init(cwq->worklist.next);
		spin_unlock_irqrestore(&cwq->lock, flags);

		BUG_ON(work->wq_data != cwq);
		clear_bit(0, &work->pending);
		f(data);

		spin_lock_irqsave(&cwq->lock, flags);
		cwq->remove_sequence++;
		wake_up(&cwq->work_done);
	}
	spin_unlock_irqrestore(&cwq->lock, flags);
	tassertMsg(preempt_count()==0,"Bad state\n");
}

typedef struct startup_s {
	struct cpu_workqueue_struct *cwq;
	struct completion done;
	const char *name;
} startup_t;

static void k42_worker_thread(uval data)
{
	struct cpu_workqueue_struct *cwq = (struct cpu_workqueue_struct*)data;
	int cpu = cwq - cwq->wq->cpu_wq;
	LinuxEnv le(SysCall);

	cwq->thread = (struct task_struct*)Scheduler::GetCurThreadPtr();
	for (;;) {
		while (!list_empty(&cwq->worklist))
			run_workqueue(cwq);

		// Clear thread marker, but then check for more work
		// We may be racing with somebody adding more work
		cwq->thread = NULL;

		// Ensure that this NULL'ing gets stored NOW
		mb();

		if (list_empty(&cwq->worklist) ||
		    !CompareAndStore64((uval64*)&cwq->thread, 0,
				       (uval64)Scheduler::GetCurThreadPtr())) {
			break;
		}
	}
}

/*
 * flush_workqueue - ensure that any scheduled work has run to completion.
 *
 * Forces execution of the workqueue and blocks until its completion.
 * This is typically used in driver shutdown handlers.
 *
 * This function will sample each workqueue's current insert_sequence number and
 * will sleep until the head sequence is greater than or equal to that.  This
 * means that we sleep until all works which were queued on entry have been
 * handled, but we are not livelocked by new incoming ones.
 *
 * This function used to run the workqueues itself.  Now we just wait for the
 * helper threads to do it.
 */
void flush_workqueue(struct workqueue_struct *wq)
{
	struct cpu_workqueue_struct *cwq;
	int cpu;

	might_sleep();

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		wait_queue_t wait;
		long sequence_needed;
		init_wait(&wait);

		if (!cpu_online(cpu))
			continue;
		cwq = wq->cpu_wq + cpu;

		spin_lock_irq(&cwq->lock);
		sequence_needed = cwq->insert_sequence;

		while (sequence_needed - cwq->remove_sequence > 0) {
			prepare_to_wait(&cwq->work_done, &wait,
					TASK_UNINTERRUPTIBLE);
			spin_unlock_irq(&cwq->lock);
			schedule();
			spin_lock_irq(&cwq->lock);
		}
		finish_wait(&cwq->work_done, &wait);
		spin_unlock_irq(&cwq->lock);
	}
}

static int create_workqueue_thread(struct workqueue_struct *wq,
				   const char *name,
				   int cpu)
{
	startup_t startup;
	struct cpu_workqueue_struct *cwq = wq->cpu_wq + cpu;
	int ret;

	spin_lock_init(&cwq->lock);
	cwq->wq = wq;
	cwq->thread = NULL;
	cwq->insert_sequence = 0;
	cwq->remove_sequence = 0;
	INIT_LIST_HEAD(&cwq->worklist);
	init_waitqueue_head(&cwq->work_done);

	return 0;
}

struct workqueue_struct *__create_workqueue(const char *name, int singlethread)
{
	int cpu, destroy = 0;
	struct workqueue_struct *wq;

	BUG_ON(strlen(name) > 10);

	wq = (typeof(wq))kmalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		return NULL;

	if (singlethread) {
		create_workqueue_thread(wq, name, cpu);
		return wq;
	}

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (!cpu_online(cpu))
			continue;
		if (create_workqueue_thread(wq, name, cpu) < 0)
			destroy = 1;
	}
	return wq;
}

static void cleanup_workqueue_thread(struct workqueue_struct *wq, int cpu)
{
	struct cpu_workqueue_struct *cwq;

	cwq = wq->cpu_wq + cpu;
	while (cwq->thread) {
		Scheduler::Yield();
	}
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	int cpu;

	flush_workqueue(wq);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu_online(cpu))
			cleanup_workqueue_thread(wq, cpu);
	}
	kfree(wq);
}

static struct workqueue_struct *keventd_wq;

int schedule_work(struct work_struct *work)
{
	return queue_work(keventd_wq, work);
}

int schedule_delayed_work(struct work_struct *work, unsigned long delay)
{
	return queue_delayed_work(keventd_wq, work, delay);
}

void flush_scheduled_work(void)
{
	flush_workqueue(keventd_wq);
}

int current_is_keventd(void)
{
	struct cpu_workqueue_struct *cwq;
	int cpu;

	BUG_ON(!keventd_wq);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (!cpu_online(cpu))
			continue;
		cwq = keventd_wq->cpu_wq + cpu;
		if (current == cwq->thread)
			return 1;
	}
	return 0;
}

void init_workqueues(void)
{
	keventd_wq = create_workqueue("events");
	BUG_ON(!keventd_wq);
}

EXPORT_SYMBOL_GPL(create_workqueue);
EXPORT_SYMBOL_GPL(queue_work);
EXPORT_SYMBOL_GPL(queue_delayed_work);
EXPORT_SYMBOL_GPL(flush_workqueue);
EXPORT_SYMBOL_GPL(destroy_workqueue);

EXPORT_SYMBOL(schedule_work);
EXPORT_SYMBOL(schedule_delayed_work);
EXPORT_SYMBOL(flush_scheduled_work);

