#ifndef __NFSHANDLE_H_
#define __NFSHANDLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSHandle.H,v 1.11 2002/10/10 13:09:22 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include "nfs.h"

class NFSHandle {
    nfsfhandle handle;
public:
    NFSHandle();
    NFSHandle(const nfsfhandle aHandle);
    NFSHandle(const NFSHandle &aHandle);
    DEFINE_NOOP_NEW(NFSHandle);
    ~NFSHandle() { /* empty body */ }

    void init(const nfsfhandle aHandle);

    void copyTo(nfsfhandle aHandle);

    const NFSHandle& operator=(const NFSHandle &aHandle);
};

#endif /* #ifndef __NFSHANDLE_H_ */
