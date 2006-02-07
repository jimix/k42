#ifndef __MEMORY_MAP_H_
#define __MEMORY_MAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memoryMap.H,v 1.9 2001/10/19 20:02:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Declare well-known symbols.
 * **************************************************************************/

#include __MINC(memoryMap.H)

#ifndef __ASSEMBLER__

extern char userPSpecificSpace;
extern char userPSpecificSpaceEnd;
extern char commonPSpecificSpace;
extern char commonPSpecificSpaceEnd;

#define userPSpecificRegionStart   ((uval) &userPSpecificSpace)
#define userPSpecificRegionEnd     ((uval) &userPSpecificSpaceEnd)
#define userPSpecificRegionSize    (userPSpecificRegionEnd - \
					userPSpecificRegionStart)

#define commonPSpecificRegionStart ((uval) &commonPSpecificSpace)
#define commonPSpecificRegionEnd   ((uval) &commonPSpecificSpaceEnd)
#define commonPSpecificRegionSize  (commonPSpecificRegionEnd - \
					commonPSpecificRegionStart)

extern void memoryMapCheck(void);

#endif	// __ASSEMBLER__

#endif /* #ifndef __MEMORY_MAP_H_ */
