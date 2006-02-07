#ifndef __LINUXGLUE_H_
#define __LINUXGLUE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxGlue.H,v 1.1 2004/02/27 17:14:32 mostrows Exp $
 *****************************************************************************/

struct LinuxVPInfo_s {
	int cpu;
	int hard_cpu;
};

typedef struct LinuxVPInfo_s LinuxVPInfo;

extern LinuxVPInfo linuxVPInfo;


#endif /* #ifndef __LINUXGLUE_H_ */
