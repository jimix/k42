#ifndef __VOLATILE_FRAME_H_
#define __VOLATILE_FRAME_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: volatileFrame.H,v 1.4 2001/10/05 21:47:54 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Assembler machinery for creating a stack frame that preserves all the
 *    volatile gpr's and fpr's.
 * **************************************************************************/

#include __MINC(volatileFrame.H)

.globl	C_TEXT(VolatileFrameEnter)
.globl	C_TEXT(VolatileFrameReturn)

#endif /* #ifndef __VOLATILE_FRAME_H_ */
