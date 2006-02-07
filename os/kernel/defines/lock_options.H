#ifndef __LOCK_OPTIONS_H_
#define __LOCK_OPTIONS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lock_options.H,v 1.1 2003/01/16 19:43:32 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines flags which enable various locking features
 * **************************************************************************/

#undef ENABLE_LOCK_CONTEXT_ASSERTIONS
#undef FAIRBLOCK_TRACK_OWNER
#undef FAIRBLOCK_HANDOFF_TIME

#define USE_LOCK_FAST_PATHS

#if defined(ENABLE_LOCK_CONTEXT_ASSERTIONS) || defined(FAIRBLOCK_TRACK_OWNER)
#undef USE_LOCK_FAST_PATHS
#endif

#endif /* #ifndef __LOCK_ASSERTIONS_H_ */
