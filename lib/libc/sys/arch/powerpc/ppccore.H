#ifndef __PPCCORE_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __PPCCORE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccore.H,v 1.23 2004/03/09 19:32:28 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine-specific declarations covering the interface
 * between assembler and C parts of the PPC implementation.
 * **************************************************************************/

// FIXME: these declarations should be in DispatcherDefault.H, probably.
extern "C" SysStatus DispatcherDefault_PPCCall(XHandle xhandle,
					       uval methnum,
					       CommID targetID);
extern "C" SysStatus DispatcherDefault_PPCAsync(XHandle xhandle,
					       uval methnum,
					       CommID targetID);

inline void
PPC_CALL(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    rc = DispatcherDefault_PPCCall(xhandle, methnum, targetID);
}
