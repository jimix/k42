#ifndef __FILE_LINUX_FORK_MANAGER_H_
#define __FILE_LINUX_FORK_MANAGER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxForkManager.H,v 1.4 2002/10/10 13:08:17 rosnbrg Exp $
 *****************************************************************************/

#include <FileLinux.H>


class IOForkManager : public Obj {
    struct Element {
	uval fileType;
	uval nextOffset;
	uval objHandleBufSize;
	char objHandleBuf;
    };
#endif /* #ifndef __FILE_LINUX_FORK_MANAGER_H_ */
