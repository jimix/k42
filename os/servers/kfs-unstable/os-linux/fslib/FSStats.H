#ifndef __FSSTATS_H_
#define __FSSTATS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSStats.H,v 1.1 2004/02/11 23:04:00 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the per-file system per-file interface,
 * object of this class is exported from the file system specific to
 * the file system independant layers.
 * **************************************************************************/

#include <sys/types.H>    // for uval
#include <sync/BLock.H>   // for BLock

class FSStats {
public:
};
#endif /* #ifndef __FSSTATS_H_ */
