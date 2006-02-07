/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClient.C,v 1.5 2003/07/23 20:19:35 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Toy Disk class implementation
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <strings.h>
#include <errno.h>
#include <KFSDebug.H>

#include "DiskClient.H"

SysStatus
DiskClient::readBlock(uval blockNumber, void *block)
{
    return LinuxBlockRead(block, dev, blockNumber, BLOCKSIZE);
}

SysStatus
DiskClient::writeBlock(uval blockNumber, void *block)
{
    return LinuxBlockWrite(block, dev, blockNumber, BLOCKSIZE);
}

SysStatus
DiskClient::readBlockPhys(uval blockNumber, uval paddr)
{
    return LinuxBlockRead((void *)paddr, dev, blockNumber, BLOCKSIZE);
}

SysStatus
DiskClient::writeBlockPhys(uval blockNumber, uval paddr)
{
    return LinuxBlockWrite((void *)paddr, dev, blockNumber, BLOCKSIZE);
}

DiskClient::DiskClient()
/*:sbd(StubObj::UNINITIALIZED),
  sbdPhys(StubObj::UNINITIALIZED)*/
{
    /* empty body */
}

uval
DiskClient::Init(char* blockDev)
{
    /* breakpoint(); */
    return 0;
}

/*static */ void
DiskClient::ClassInit(VPNum vp)
{
}


/*static*/ SysStatus
DiskClient::Create(DiskClientRef &dcr, char* blockDev)
{
    DiskClient *dc = new DiskClient;

    SysStatus rc = dc->init(blockDev);

    _IF_FAILURE_RET(rc);
    
    dcr = dc->getRef();

    return 0;
}

SysStatus
DiskClient::init(char* blockDev)
{
    SysStatus rc=0;

    dev = *(kdev_t *)blockDev;
    size = LinuxGetHardsectSize(dev);
    err_printf("(DiskClient::init) DEV=%d, size=%ld\n", dev, size);

    return rc;
}
