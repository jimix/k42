#ifndef __KFS_DEBUG_H_
#define __KFS_DEBUG_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSDebug.H,v 1.18 2003/12/22 16:00:31 dilma Exp $
 *****************************************************************************/

/*
 * kfsDebug.H
 *
 *    This file contains all the macros and things used for debugging.
 */


#define KFS_DEBUG

// KFS_DPRINTF() prints only when debuging is enabled
#if defined(KFS_DEBUG) && !defined(NDEBUG)

class DebugMask {
public:
    enum {
	FS_FILE_KFS                = 0x00000001,
	FS_FILE_KFS_READ_BLOCK     = 0x00000002,
	FS_FILE_KFS_WRITE_BLOCK    = 0x00000004,
	LSO_BASIC                  = 0x00000008,
	LSO_BASIC_DIR              = 0x00000010,
	LSO_BASIC_DIR_ENTRIES      = 0x00000020,
	PSO_BASIC_RW               = 0x00000040,
	PSO_SMALL                  = 0x00000080,
	PSO_SMALL_RW               = 0x00000100,
	PSO_SMALL_TRUNCATE         = 0x00000200,
	PSO_DISK_BLOCK             = 0x00000400,
	PSO_REALLOC_EXTENT         = 0x00000800,
	PSO_REALLOC_EXTENT_RW      = 0x00001000,
	RECORD_MAP                 = 0x00002000,
	SUPER_BLOCK                = 0x00004000,
	FSCK                       = 0x00008000,
	FILE_SYSTEM_KFS            = 0x00010000,
	SERVER_OBJECT              = 0x00020000,
	SERVER_FILE                = 0x00040000,
	SERVER_FILE_W              = 0x00080000,
	SERVER_FILE_R              = 0x00100000,
	SERVER_FILE_DIR            = 0x00200000,
	OBJ_TOKEN                  = 0x00400000,
	DATA_BLOCK                 = 0x00800000,
	SNAP                       = 0x01000000,
	SNAP_RW                    = 0x02000000,
	LINUX                      = 0x04000000,
	CACHE                      = 0x08000000,
	CACHE_SYNC                 = 0x10000000,
	ALL                        = 0xffffffff,
    };
    static uval32 Mask;
    static uval32 GetDebugMask() { return Mask; }
    static void SetDebugMask(uval32 m) { Mask = m; }
    static void PrintDebugClasses(uval &currMask);
};

#define KFS_DPRINTF(mask, strargs...) \
          if (DebugMask::GetDebugMask() & mask) err_printf(strargs);
#else /* #ifdef KFS_DEBUG */
class DebugMask {
public:
    static void PrintDebugClasses(uval &currMask);
};
#define KFS_DPRINTF(mask, strargs...)
#endif /* #ifdef KFS_DEBUG */

#endif /* #ifndef __KFS_DEBUG_H_ */
