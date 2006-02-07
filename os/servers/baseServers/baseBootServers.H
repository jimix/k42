#ifndef __BASE_BOOT_SERVERS_H_
#define __BASE_BOOT_SERVERS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: baseBootServers.H,v 1.5 2002/11/22 13:53:54 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Mechanisms needed in booting an initial set of
 * servers.
 * **************************************************************************/

#include <misc/utilities.H>

#ifndef KERNEL
#include <stdlib.h>
#include <stdio.h>
#else
#include <misc/baseStdio.H>
#define atoi baseAtoi
#define sprintf baseSprintf
#endif

#include <sys/bootServers.H>

#endif /* #ifndef __BASE_BOOT_SERVERS_H_ */
