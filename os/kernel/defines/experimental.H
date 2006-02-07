#ifndef __EXPERIMENTAL_H_
#define __EXPERIMENTAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: experimental.H,v 1.25 2005/05/12 07:02:40 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines varios flags which configure experimental
 * features for experimentation.
 * **************************************************************************/

//#define ENABLE_FCM_SWITCHING

//#define NO_PADDED_ALLOCATIONS

//#define ALLOW_NO_NONBLOCKING


/* Used in misc/DHashTable.H. We do want to turn this on eventually, but
 * the code has not been throughly reviewed, it has a known bug (when the
 * FCM runs with this, SDET on large configuration will hung).
 * Also, there are subtle things going with trying to shrink the talbe
 * using an RCU scheme for RequestCount.
 */ 
//#define DHASH_RCU_REQUESTCOUNT

/* Used to turn off/on definition of class RequestCountWithRCU in
 * lib/libc/sync/RequestCount.H
 */
//#define RCU_REQUEST_COUNT

/* To turn on use of FACTORIES */
//#define USE_PROCESS_FACTORIES

// #define PMLEAF_CHUNK_ALLOCATION


#endif /* #ifndef __EXPERIMENTAL_H_ */
