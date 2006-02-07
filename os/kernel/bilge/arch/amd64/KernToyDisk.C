/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernToyDisk.C,v 1.8 2003/01/23 20:33:06 dilma Exp $
 *************************************************************************** */


static void inline InitMachineSpecificKernToy(uval &pageBufAddr)
{
  tassertWrn(0,"FIXME XXX\n");
}

/* virtual */ SysStatus
KernToyDisk::_simDiskValid()
{
  tassertWrn(0,"FIXME XXX");
  return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_readPhys(unsigned long, unsigned long)
{
  tassertWrn(0,"FIXME XXX");
  return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_readVirtual(unsigned long, char*, unsigned long)
{
  tassertWrn(0,"FIXME XXX");
  return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_writePhys(unsigned long, unsigned long)
{
  tassertWrn(0,"FIXME XXX");
  return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_writeVirtual(unsigned long, char const*, unsigned long)
{
  tassertWrn(0,"FIXME XXX");
  return 0;
}

