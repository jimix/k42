/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSDebug.C,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "KFSDebug.H"

#if defined(KFS_DEBUG) && !defined(NDEBUG)
/* static */ uval32 DebugMask::Mask = 0;
// /* static */ uval32 DebugMask::Mask = DebugMask::ALL;

/* static */ void
DebugMask::PrintDebugClasses(uval &currMask)
{
#ifdef KFS_DEBUG
    currMask = Mask;
    err_printf("Current debug classes available for KFS:\n");
    err_printf("\tFS_FILE_KFS                : 0x%lx\n", (uval) FS_FILE_KFS);
    err_printf("\tFS_FILE_KFS_READ_BLOCK     : 0x%lx\n",
	       (uval) FS_FILE_KFS_READ_BLOCK);
    err_printf("\tFS_FILE_KFS_WRITE_BLOCK    : 0x%lx\n",
	       (uval) FS_FILE_KFS_WRITE_BLOCK);
    err_printf("\tLSO_BASIC                  : 0x%lx\n", (uval) LSO_BASIC);
    err_printf("\tLSO_BASIC_DIR              : 0x%lx\n", (uval) LSO_BASIC_DIR);
    err_printf("\tLSO_BASIC_DIR_ENTRIES      : 0x%lx\n",
	       (uval) LSO_BASIC_DIR_ENTRIES);
    err_printf("\tPSO_BASIC_RW               : 0x%lx\n", (uval) PSO_BASIC_RW);
    err_printf("\tPSO_SMALL                  : 0x%lx\n", (uval) PSO_SMALL);
    err_printf("\tPSO_SMALL_RW               : 0x%lx\n", (uval) PSO_SMALL_RW);
    err_printf("\tPSO_SMALL_TRUNCATE         : 0x%lx\n",
	       (uval) PSO_SMALL_TRUNCATE);
    err_printf("\tPSO_DISK_BLOCK             : 0x%lx\n",
	       (uval) PSO_DISK_BLOCK);
    err_printf("\tPSO_REALLOC_EXTENT         : 0x%lx\n",
	       (uval) PSO_REALLOC_EXTENT);
    err_printf("\tPSO_REALLOC_EXTENT_RW      : 0x%lx\n", (uval) RECORD_MAP);
    err_printf("\tRECORD_MAP                 : 0x%lx\n", (uval) RECORD_MAP);
    err_printf("\tSUPER_BLOCK                : 0x%lx\n", (uval) SUPER_BLOCK);
    err_printf("\tFSCK                       : 0x%lx\n", (uval) FSCK);
    err_printf("\tFILE_SYSTEM_KFS            : 0x%lx\n",
	       (uval) FILE_SYSTEM_KFS);
    err_printf("\tSERVER_OBJECT              : 0x%lx\n", (uval) SERVER_OBJECT);
    err_printf("\tSERVER_FILE                : 0x%lx\n", (uval) SERVER_FILE);
    err_printf("\tSERVER_FILE_W              : 0x%lx\n", (uval) SERVER_FILE_W);
    err_printf("\tSERVER_FILE_R              : 0x%lx\n", (uval) SERVER_FILE_R);
    err_printf("\tSERVER_FILE_DIR            : 0x%lx\n",
	       (uval) SERVER_FILE_DIR);
    err_printf("\tOBJ_TOKEN                  : 0x%lx\n", (uval) OBJ_TOKEN);
    err_printf("\tDATA_BLOCK                 : 0x%lx\n", (uval) DATA_BLOCK);
    err_printf("\tSNAP                       : 0x%lx\n", (uval) SNAP);
    err_printf("\tSNAP_RW                    : 0x%lx\n", (uval) SNAP_RW);
    err_printf("\tLINUX                      : 0x%lx\n", (uval) LINUX);
    err_printf("\tALL                        : 0x%lx\n", (uval) ALL);

#else // #if defined(KFS_DEBUG) && !defined(NDEBUG)
    err_printf("KFS_DEBUG is not on\n");
#endif // #if defined(KFS_DEBUG) && !defined(NDEBUG)
}

#endif
