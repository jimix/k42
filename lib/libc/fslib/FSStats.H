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
 * $Id: FSStats.H,v 1.3 2003/08/02 21:28:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the per-file system per-file interface,
 * object of this class is exported from the file system specific to
 * the file system independant layers.
 * **************************************************************************/
class FSStats {
    typedef enum {LESS_3K, LESS_10K, LESS_100K, LESS_1M,
		  NOT_LESS_1M} FileSizes;
    static void updateSizeArray(uval array[], uval size);

public:

    typedef enum {LOOKUP_FAILURE,
		  LOOKUP_SUCCESS,
		  CLIENT_CREATED,
		  CLIENT_CREATED_LAZY_INIT,
		  CLIENT_CREATED_NON_SHARED,
		  CLIENT_CREATED_SHARED,
		  CLIENT_CREATED_FIXED_SHARED,
		  CLIENT_BECAME_NON_SHARED,
		  CLIENT_BECAME_SHARED,
		  CLIENT_SWITCH,
		  OPEN,
		  OPEN_RDONLY,
		  DUP,
		  REGISTER_CALLBACK,
		  ACK_USETYPE,
		  LENGTH_OFFSET,
		  OPEN_SIZE,
		  CLOSE_SIZE,
		  MAX_WRITE_SIZE_SHARED,
		  MAX_WRITE_SIZE_NON_SHARED,
		  MAX_STAT_TYPE} StatType;
    typedef BLock StatLockType;
    StatLockType      statLock;			// lock on stats

    uval stats[MAX_STAT_TYPE+1];
    uval nb_max_write_shared;
    uval second_max_write_shared;
    uval openSizes[5], closeSizes[5];

    void incStat(StatType type, uval extra_arg = 0);
    void printStats();
    void initStats();
};
#endif /* #ifndef __FSSTATS_H_ */
