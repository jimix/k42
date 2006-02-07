#ifndef __GENERIC64_ASM_H_
#define __GENERIC64_ASM_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file
 * LICENSE.html in the top-level directory for more details.
 *
 * $Id: asm.h,v 1.3 2001/10/05 21:47:58 peterson Exp $
 *************************************************************************** */

#define GLBL_LABEL(x) .globl x; x:

#define C_TEXT(x) x
#define C_DATA(x) x

#define CODE_END(x)

#define C_TEXT_END(x) \
        CODE_END(C_TEXT(x))

#define C_TEXT_ENTRY(x) \
        .text; .align 8; GLBL_LABEL(C_TEXT(x))

#define C_DATA_ENTRY(x) \
	GLBL_LABEL(C_DATA(x))

#endif /* #ifndef __GENERIC64_ASM_H_ */
