#ifndef __PSOTYPES_H_
#define __PSOTYPES_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PSOTypes.H,v 1.12 2003/11/25 03:41:42 lbsoares Exp $
 *****************************************************************************/

// Different object types
enum PsoType {
    OT_RESERVED = 0,
    OT_WBUF = 1,
    OT_PRIM_UNIX = 2,	/* primative unix like file <- replace this */
    OT_PRIM_UNIX_META = 3,   /* primative unix like file for meta-data */
    OT_STRIPED = 4,	/* striped  file <- replace this */

    // Ones described in dissertation
    OT_BASIC_RW = 5,       // A basic per/disk read/write object
    OT_BASIC_SPARSE = 6,   // A basic per/disk sparse object
    OT_BASIC_DENSE = 7,    // A basic per/disk dense object
    OT_NM_SMALL = 8,       // A non-mapped small object
    OT_RECORD_MAP = 9,     // A non-mapped record store object
    OT_COMP_STR = 10,      // A composite striped object
    OT_COMP_REP = 11,      // A composite replicated object
    OT_COMP_DIS = 12,
    OT_COMP_CHCK = 13,
    OT_COMP_PAR = 14,
    OT_COMP_AS = 15,

    /* reserved for LSOs, cause flush must operate on either */
    OT_LSO_BASIC = 16,
    OT_LSO_BASIC_DIR = 17,
    OT_LSO_BASIC_LNK = 18,
    OT_LSO4 = 19,

    OT_DISK_BLOCK = 20,    // Low-level disk-based object
    OT_BASIC_EXTENT = 21,

    OT_SYMLINK_EXT = 22,
};

#endif /* #ifndef __PSOTYPES_H_ */
