#ifndef __EXT2_CONVERSION_H
#define __EXT2_CONVERSION_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Ext2Conversion.H,v 1.11 2004/08/20 02:06:56 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

extern SysStatus callStat(void *m, void *d, void *gstatus);
extern SysStatusUval callGetDents(void *dentry, uval &cookie,
				  void *dirbuf, uval len);
extern "C" {
#include <linux/k42fs.h>
};

#endif // __EXT2_CONVERSION_H
