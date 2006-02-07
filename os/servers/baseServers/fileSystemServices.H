#ifndef __FILE_SYSTEM_SERVICES_H
#define __FILE_SYSTEM_SERVICES_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fileSystemServices.H,v 1.10 2004/06/30 16:08:03 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

extern void StartNFS(uval rcPtr);
extern void StartFileSystemServices();

#ifdef KFS_ENABLED
extern void RunInitKFS(uval rcPtr);
extern SysStatus InitKFS();
#endif // #ifdef KFS_ENABLED

#if EXT2_ENABLED
extern void StartExt2(uval rcPtr);
#endif //#ifdef EXT2_ENABLED

#endif // #ifndef __FILE_SYSTEM_SERVICES_H
