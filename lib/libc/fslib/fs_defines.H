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
 * $Id: fs_defines.H,v 1.26 2005/09/06 20:40:12 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines that control debugging in file system (nothing
 * relevant in terms of functionality)
 * **************************************************************************/

/* Define the macro below if you want to make FSCreds::Acquire() and Release
 * no ops; all code doing credential checking is then commented out.
 */
//#define NO_CREDENTIAL_CHECKING

// The following macro controls if the scheme in which a file determines
// its shared vs non-shared behavior. LAZY means that a client is not
// determined to be NON_SHARED until it actually issues an operation on the
// file: its useType state is LAZY_INIT. When it does issue an operation
// (read, write, etc) it contacts the server to register a call back and
// it gets back information of the behavior it should have (SHARED, NON_SHARED)
// If LAZY_SHARING_SETUP is not defined, behavior is defined when the file
// is open, so if multiple clients open the file but don't actually use it
// we do pay for making both of them SHARED

#define LAZY_SHARING_SETUP

//#define TRACE_HOT_SWAP
//#define DILMA_DEBUG_DESTROY
//#define DEBUG_TRACE_SERVER_FILE
//#define DEBUG_ONE_FILE

// use line below to have baseServers allocating maximum file size
// when caching small files in the ServerFile object
#define SMALL_FILE_ALLOC_ONE_BIG_CHUNK

//comment line below to keep cache of directory entries in a hash table
//#define CACHE_LINEAR_LIST

/*
 * These are the fs-independent mount-flags. The names and values were
 * copied from Linux definitions in include/fs.h.
 */
#define MS_RDONLY	 1	/* Mount read-only */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#include <defines/mem_debug.H>

//#define DEBUG_ESTALE
//#define DEBUG_FRINTERACTION
//#define DILMA_DEBUG_DIRLINUXFS
//#define DEBUG_USE_TYPE
//#define DILMA_DEBUG
//#define DILMA_DEBUG_SWITCH
//#define DILMA_DEBUG_SMALLFILES

#define USING_READER_WRITER_LOCK_FOR_DIRECTORIES

// The following maintains full path names in ServerFile objects
//#define HACK_FOR_FR_FILENAMES

//#define GATHERING_STATS

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

