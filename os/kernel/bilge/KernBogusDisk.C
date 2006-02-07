/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernBogusDisk.C,v 1.1 2003/11/08 17:29:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Bogus disk interface
 * **************************************************************************/

#include "kernIncs.H"
#include "KernBogusDisk.H"
#include "mem/PageAllocatorKernPinned.H"
#include <meta/MetaKernBogusDisk.H>

// machine specific implementation of KernBogusDisk simulator specific
#include __MINC(KernBogusDisk.C)

/*static*/ SysStatus
KernBogusDisk::Create(KernBogusDisk* &obj, uval diskID)
{
    MetaKernBogusDisk::init();
    obj = new KernBogusDisk(diskID);
    obj->init();
    return obj->_simDiskValid();
}

void
KernBogusDisk::init()
{
    pageBufLock.init();
    InitMachineSpecificKernBogus(pageBufAddr);
}
