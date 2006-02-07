/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernToyDisk.C,v 1.12 2003/11/08 17:30:13 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PowerPC specific toy disk implementation
 * **************************************************************************/
#include "misc/arch/powerpc/simSupport.H"
#include "mem/PageAllocatorKernPinned.H"
#include <sys/KernelInfo.H>
#include "bilge/arch/powerpc/BootInfo.H"

static void inline InitMachineSpecificKernToy(uval &pageBufAddr)
{
    // a single page for copying virtual through
    SysStatus rc;
    if (KernelInfo::OnSim()) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(pageBufAddr, 
							      PAGE_SIZE);
	err_printf("KernToyDisk: allocated page %lx, pa-%lx\n", pageBufAddr, 
		   PageAllocatorKernPinned::virtToReal(pageBufAddr));
	tassert(_SUCCESS(rc), err_printf("can't alloc page for toy disk\n"));
    }
}

/* virtual */ SysStatus
KernToyDisk::_simDiskValid() 
{
    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return
    if (KernelInfo::OnSim()) {
	sval rc;
	// put in a check here that disk is valid
	*((uval*)(pageBufAddr)) = 0;
	rc =
	    SimOSSupport(SimDiskReadK42K, // code for "disk read"
			 simosDiskID,     // id of disk for this KernToyDisk
			 0,		// byte offset which to read
			 PageAllocatorKernPinned::virtToReal(
			     pageBufAddr),	//  address of buffer
			 4096);	// number of bytes to be read
	if (rc >=0 ) {
	    // disk is valid
	    return 0;
	}
    }
    return _SERROR(1709, 0, ENXIO);
}


/* virtual */ SysStatusUval
KernToyDisk::_writeVirtual(uval offset, const char* buf, uval length)
{
    sval dataWritten;

    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return
#if 0
    err_printf("In _writeVirtual: obj %p simosDiskID is %ld\n", this,
	       simosDiskID);
#endif
    memcpy((void *)pageBufAddr, buf, length);
    dataWritten = 
	SimOSSupport(SimDiskWriteK42K,	// code for "disk write"
		     simosDiskID,
		     offset,		// byte offset which to write
		     PageAllocatorKernPinned::virtToReal(
			 pageBufAddr),	//  address of buffer
		     length);		// number of bytes to be written

    tassert(((uval)dataWritten==length), 
	    err_printf("data written = %ld, while reqlen = %ld\n",
		       dataWritten, length));
    return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_readVirtual(uval offset, char* buf, uval buflength)
{
    sval dataRead;

    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return
#if 0
    err_printf("In _readVirtual: obj %p simosDiskID is %ld\n", this,
	       simosDiskID);
#endif

    // touch the page to get in page table
    *((uval*)(pageBufAddr)) = 0;
    dataRead =
	SimOSSupport (SimDiskReadK42K,	// code for "disk read"
		      simosDiskID,
		      offset,		// byte offset which to read
		      PageAllocatorKernPinned::virtToReal(
			  pageBufAddr),	//  address of buffer
		      buflength);		// number of bytes to be read


    // zero whatever part of buffer not read
    if (dataRead < 0) {
	memset((void *)buf, 0, buflength);
    } else {
	memcpy((void *)buf, (void *)pageBufAddr, dataRead);
	if (dataRead < (sval)buflength) {
	    memset((void *)(buf+dataRead), 0, (buflength-dataRead));
	} 
    }

    return buflength;
}


/* virtual */ SysStatusUval
KernToyDisk::_writePhys(uval offset, uval paddr)
{
    // not dealing with errors
    (void)SimOSSupport(SimDiskWriteK42K,// code for "disk write"
		       simosDiskID,
		       offset,		// byte offset which to write
		       paddr,		//  address of buffer
		       PAGE_SIZE);	// number of bytes to be written
    return 0;
}

/* virtual */ SysStatusUval
KernToyDisk::_readPhys(uval offset, uval paddr)
{
    // not handling errors
    (void)SimOSSupport (SimDiskReadK42K,// code for "disk read"
			simosDiskID,
			offset,		// byte offset which to read
			paddr,		// phys address of buffer
			PAGE_SIZE);	// number of bytes to be read
    return 0;
}
