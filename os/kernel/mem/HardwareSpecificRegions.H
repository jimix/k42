#ifndef __HARDWARE_SPECIFIC_REGIONS_H_
#define __HARDWARE_SPECIFIC_REGIONS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HardwareSpecificRegions.H,v 1.5 2001/10/05 21:49:57 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines any kernel regions needed by specific hardware.
 * **************************************************************************/

class HardwareSpecificRegions {
#define INCLASS
#include __MINC(HardwareSpecificRegions.H)
public:
    static void ClassInit(VPNum vp);
};
#undef INCLASS
#include __MINC(HardwareSpecificRegions.H)

#endif /* #ifndef __HARDWARE_SPECIFIC_REGIONS_H_ */
