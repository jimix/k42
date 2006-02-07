/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernToyDisk.C,v 1.9 2003/10/14 17:56:06 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Disk interface for toy file system
 * **************************************************************************/
#include "kernIncs.H"
#include "KernToyDisk.H"
#include "mem/PageAllocatorKernPinned.H"
#include <meta/MetaKernToyDisk.H>

// machine specific implementation of KernToyDisk simulator specific
#include __MINC(KernToyDisk.C)

/*static*/ void
KernToyDisk::ClassInit()
{
    MetaKernToyDisk::init();
}

/*static*/ SysStatus
KernToyDisk::Create(KernToyDisk* &obj, uval simosDisk)
{
    MetaKernToyDisk::init();
    obj = new KernToyDisk(simosDisk);
    obj->init();
    return obj->_simDiskValid();
}

void
KernToyDisk::init()
{
    pageBufLock.init();
    InitMachineSpecificKernToy(pageBufAddr);
}
