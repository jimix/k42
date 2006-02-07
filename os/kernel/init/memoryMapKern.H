#ifndef __MEMORY_MAP_KERN_H_
#define __MEMORY_MAP_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memoryMapKern.H,v 1.7 2001/10/05 21:49:41 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Declare well-known symbols.
 * **************************************************************************/

#include <sys/memoryMap.H>

extern char kernelPSpecificSpace;
extern char kernelPSpecificSpaceEnd;

#define kernelPSpecificRegionStart ((uval) &kernelPSpecificSpace)
#define kernelPSpecificRegionEnd   ((uval) &kernelPSpecificSpaceEnd)
#define kernelPSpecificRegionSize  (kernelPSpecificRegionEnd - \
					kernelPSpecificRegionStart)

extern void memoryMapCheckKern(VPNum vp);

#endif /* #ifndef __MEMORY_MAP_KERN_H_ */
