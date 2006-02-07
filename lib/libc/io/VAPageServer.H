#ifndef __VAPAGE_SERVER_H_
#define __VAPAGE_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VAPageServer.H,v 1.16 2005/01/10 15:27:32 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Interface that the kernel speaks to an NFS like
 * file system.
 ****************************************************************************/
#include <cobj/ObjectHandle.H>

class VAPageServer : public Obj {
__xprivate:

    // calls from FR
    // put a specified page frame data back to  offset in file,
    virtual __async SysStatus _startWrite(__in uval virtAddr,
					  __in uval objOffset,
					  __in uval len,
					  __XHANDLE xhandle)=0;

    virtual __async SysStatus _startFillPage(__in uval virtAddr,
					     __in uval objOffset,
					     __XHANDLE xhandle)=0;

    virtual __async SysStatus _frIsNotInUse(uval fileToken)=0;

    virtual __async SysStatus _startIO(__XHANDLE xhandle)=0;
};

#endif /* #ifndef __VAPAGE_SERVER_H_ */
