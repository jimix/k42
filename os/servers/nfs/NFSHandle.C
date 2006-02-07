/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSHandle.C,v 1.7 2002/10/10 13:09:22 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "NFSHandle.H"


NFSHandle::NFSHandle()
{
}

NFSHandle::NFSHandle(const nfsfhandle aHandle)
{
    memcpy(handle, aHandle, FHSIZE);
}

NFSHandle::NFSHandle(const NFSHandle &aHandle)
{
    memcpy(&handle, &aHandle.handle, FHSIZE);
}

void
NFSHandle::init(const nfsfhandle aHandle)
{
    memcpy(handle, aHandle, FHSIZE);
}

void
NFSHandle::copyTo(nfsfhandle aHandle)
{
    memcpy(aHandle, handle, FHSIZE);
}

const NFSHandle&
NFSHandle::operator=(const NFSHandle &aHandle)
{
    memcpy(&handle, &aHandle.handle, FHSIZE);

    return *this;
}
