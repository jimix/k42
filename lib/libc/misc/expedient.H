#ifndef __EXPEDIENT_H_
#define __EXPEDIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: expedient.H,v 1.7 2001/10/05 21:47:52 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

#include __MINC(expedient.H)

#ifndef __ASSEMBLER__

extern "C" void GotoLegitimateAsm(ExpRegs *, code &);
extern "C" void CallLegitimateAsm(ExpRegs *, code &);
extern "C" void CallLegitimateC(ExpRegs *, uval);

#else /* #ifndef __ASSEMBLER__ */

.globl	C_TEXT(GotoExpedientC)
.globl	C_TEXT(CallExpedientC)

#endif /* #ifndef __ASSEMBLER__ */

#endif /* #ifndef __EXPEDIENT_H_ */
