#ifndef __EXT_REGS_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __EXT_REGS_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: extRegs.H,v 1.1 2004/03/09 19:32:28 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This file defines a logical extension to the register
 *                     set implemented using a processor-specific read-write
 *                     page mapped into every address space.
 * **************************************************************************/

// No machine-dependent extended registers needed on powerpc.
struct ExtRegsArch {
};
