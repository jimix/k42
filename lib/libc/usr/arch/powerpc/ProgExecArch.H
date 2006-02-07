#ifndef __ProgExec_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ProgExec_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExecArch.H,v 1.3 2004/12/21 15:05:16 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of ProgExec.H
 * **************************************************************************/

#if defined(INCLASS)
enum {AllocStart32Bit=0, AllocEnd32Bit=0x10000000, Exec32Bit=1};
static uval Is32Bit() {
    return ExecXferInfo->exec.prog.archFlags & Exec32Bit;
};
#elif defined(OUTOFCLASS)
/* nothing need outofclass for powerpc */
#endif /* #if defined(INCLASS) */

















