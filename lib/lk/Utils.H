#ifndef __UTILS_H_
#define __UTILS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Utils.H,v 1.8 2004/09/30 11:21:54 cyeoh Exp $
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

extern int Linux_OnSim();
extern int Linux_InetAddr(const char *addr);
extern int printk(const char *fmt, ...);
extern int pow2(int x);

#ifdef __cplusplus

//
// Sometimes we want to put a callback on an LK wait queue; this is the
// object we use
//
#define K42_WAIT_NOTIFY	    0x80000000
#define K42_WAIT_PERSISTENT 0x40000000
struct K42WaitQElm: public wait_queue_t {
    K42WaitQElm() {
	init_waitqueue_entry(this, NULL);
	INIT_LIST_HEAD(&task_list);
    }
    virtual void wakeup(unsigned long arg) = 0;
};

//Special version that flags wait element as being a callback
extern void add_wait_queue_k42notify(wait_queue_head_t *q,
				     K42WaitQElm *obj,
				     unsigned long arg,
				     unsigned long persistent);

} // extern "C"

#endif /* #ifdef __cplusplus */
#endif /* #ifndef __UTILS_H_ */
