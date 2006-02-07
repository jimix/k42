/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernBogusDisk.C,v 1.1 2003/11/08 17:30:12 dilma Exp $
 *************************************************************************** */


static void inline InitMachineSpecificKernToy(uval &pageBufAddr)
{
}

/* virtual */ SysStatus
KernBogusDisk::_simDiskValid()
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_getBlockSize()
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_getDevSize()
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_readPhys(unsigned long, unsigned long)
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_readVirtual(unsigned long, char*, unsigned long)
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_writePhys(unsigned long, unsigned long)
{
    return 0;
}

/* virtual */ SysStatusUval
KernBogusDisk::_writeVirtual(unsigned long, char const*, unsigned long)
{
    return 0;
}

