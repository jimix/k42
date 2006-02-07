#ifndef __FS_DEFINES_H
#define __FS_DEFINES_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fs_defines.H,v 1.4 2003/08/18 17:46:45 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines that control debugging in file system (nothing
 * relevant in terms of functionality)
 * **************************************************************************/


#define HDRW_DISK_NAME /dev/scsi/host0/bus0/target1/lun0

/*
 * These are the fs-independent mount-flags. The names and values were
 * copied from Linux definitions in include/fs.h.
 */
#define MS_RDONLY	 1	/* Mount read-only */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

// #include <defines/mem_debug.H>


#define USING_READER_WRITER_LOCK_FOR_DIRECTORIES
// The following maintains full path names in ServerFile objects
// #define HACK_FOR_FR_FILENAMES

// The following macro is used in lib/libc/misc/StringTable.I, to make
// sure that the DynamicStrTable used to store cached directory entries
// is initialized large enough to avoid resizing. This hack is necessary
// because current implementation of DirLinuxFS objects retrieve the
// NameHolder pointer from the DynamicStrTable and keep using it after
// releasing the Directory lock (so if a resize occurs as the pointer is
// still in use, it becomes meaningless). We're wasting memory for now
// instead of fixing the code because we intend to change the data
// structure for cached entries to be a distributed hash table.
#define MAX_NUMBER_STR_TABLE_ENTRIES 200

//#define GATHERING_STATS

//#define DILMA_DEBUG
//#define DILMA_DEBUG_SWITCH
//#define DILMA_DEBUG_SMALLFILES

//#define DESTRUCTION_DEAMON_ON
//#define DESTRUCTION_DAEMON_STRESS_TESTING

//#define INSTRUMENTING_FILE_SHARING

//#define DILMA_DEBUG_SDET

/* defining MOUNT_NFS_OTHER_ROOT implies that an additional NFS file system is
 * created as /nfs-otherRoot. This file system is used by the testing program
 * kitch-linux/tests/linux/nfsCaching.c: /nfs-otherRoot is used for faking
 * remote changes on the same files being accessed by the usual/normal NFS
 * tree */
//#define MOUNT_NFS_OTHER_ROOT

//#define DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR

//#define SHARED_FILE_USE_ONLY

#endif /* ifndef __FS_DEFINES_H */
