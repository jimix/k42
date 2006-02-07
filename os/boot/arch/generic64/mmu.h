#ifndef __GENERIC64_MMU_H_
#define __GENERIC64_MMU_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmu.h,v 1.2 2001/10/05 21:48:55 peterson Exp $
 *************************************************************************** */

 struct PTE {
     uval64 frame_number;
 };
#endif /* #ifndef __GENERIC64_MMU_H_ */
