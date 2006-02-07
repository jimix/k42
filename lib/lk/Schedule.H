#ifndef __SCHEDULE_H_
#define __SCHEDULE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Schedule.H,v 1.2 2001/10/05 21:49:20 peterson Exp $
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

void __k42_add_wait_queue(void *head);
void __k42_remove_wait_queue(void *head);

#ifdef __cplusplus
} // extern "C"
#endif /* #ifdef __cplusplus */
#endif /* #ifndef __SCHEDULE_H_ */
