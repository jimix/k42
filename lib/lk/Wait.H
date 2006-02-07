#ifndef _K42_WAIT_H
#define _K42_WAIT_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Wait.H,v 1.1 2004/02/27 17:14:31 mostrows Exp $
 *****************************************************************************/

// It's hard to get just this without corrupting entire inclusion
// namespace So we'll copy this out of include/linux/wait.h and
// include just the parts we want
typedef struct __wait_queue wait_queue_t;
typedef int (*wait_queue_func_t)(wait_queue_t *wait, unsigned mode, int sync);
extern int default_wake_function(wait_queue_t *wait, unsigned mode, int sync);

struct list_head {
	struct list_head *next, *prev;
};

struct __wait_queue {
	unsigned int flags;
#define WQ_FLAG_EXCLUSIVE	0x01
	struct task_struct * task;
	wait_queue_func_t func;
	struct list_head task_list;
};

typedef struct __wait_queue_head wait_queue_head_t;

extern "C" void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
extern "C" void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);

#endif //_K42_WAIT_H
