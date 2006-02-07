/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernBogusDisk.C,v 1.3 2003/12/22 15:58:49 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PowerPC specific bogs disk implementation
 * **************************************************************************/
#include "misc/arch/powerpc/simSupport.H"
#include "mem/PageAllocatorKernPinned.H"
#include <sys/KernelInfo.H>
#include "bilge/arch/powerpc/BootInfo.H"

static void inline InitMachineSpecificKernBogus(uval &pageBufAddr)
{
    // a single page for copying virtual through
    SysStatus rc;
    if (KernelInfo::OnSim() == SIM_MAMBO) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(pageBufAddr, 
							      PAGE_SIZE);
#if 0
	err_printf("KernBogusDisk: allocated page %lx, pa-%lx\n", pageBufAddr, 
		   PageAllocatorKernPinned::virtToReal(pageBufAddr));
#endif
	tassertMsg(_SUCCESS(rc), "can't alloc page for toy disk\n");
    }
}

/* virtual */ SysStatus
KernBogusDisk::_simDiskValid() 
{
    SysStatus rc = 0;
    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return
    if (KernelInfo::OnSim() == SIM_MAMBO) {
	int ret = MamboBogusDiskInfo(BD_INFO_STATUS, diskID);
	// is this check right?
	if (ret != 1) {
	    rc = _SERROR(2754 , 0, ENXIO);
	}
    }
    return rc;
}

/* virtual */ SysStatusUval
KernBogusDisk::_getBlockSize() 
{
    if (KernelInfo::OnSim() == SIM_MAMBO) {
	return _SRETUVAL(MamboBogusDiskInfo(BD_INFO_BLKSZ, diskID));
    }
    return _SERROR(2583, 0, 0);
}

/* virtual */ SysStatusUval
KernBogusDisk::_getDevSize() 
{
    if (KernelInfo::OnSim() == SIM_MAMBO) {
	return _SRETUVAL(MamboBogusDiskInfo(BD_INFO_DEVSZ, diskID));
    }
    return _SERROR(2743, 0, 0);
}


/* virtual */ SysStatusUval
KernBogusDisk::_writeVirtual(uval offset, const char* buf, uval len)
{
#if 0
    err_printf("In KernBogusDisk::_writeVirtual: offset %ld, obj %p,"
	       " diskID is %ld\n", offset, this, diskID);
#endif

    passertMsg(len <= BD_MAX_BUF, "Not dealing with len > BD_MAX_BUF "
	       "(len is %ld)\n", len);
    passertMsg(offset % BD_SECT_SZ == 0, "Not dealing with this type of "
	       "offset yet (objOffset 0x%lx)\n", offset);
    passertMsg(len % BD_SECT_SZ == 0, "Not dealint with this len (%ld) yet\n",
	       len);

    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return

    memcpy((void *)pageBufAddr, buf, len);
    (void) MamboBogusDiskWrite(diskID, (void*) pageBufAddr,
			       offset / BD_SECT_SZ, len / BD_SECT_SZ);
    // FIXME: not tolerating any error ...
    return len;
}

/* virtual */ SysStatusUval
KernBogusDisk::_readVirtual(uval offset, char* buf, uval buflength)
{
    passertMsg(buflength <= BD_MAX_BUF, "Not dealing with len > BD_MAX_BUF "
	       "(len is %ld)\n", buflength);
    passertMsg(offset % BD_SECT_SZ == 0, "Not dealing with this type of "
	       "offset yet (objOffset 0x%lx)\n", offset);
    passertMsg(buflength % BD_SECT_SZ == 0, "Not dealint with this len (%ld) yet\n",
	       buflength);

    int dataRead;

    AutoLock<BLock> al(pageBufLock); // locks now, unlocks on return

#if 0
    err_printf("In _readVirtual: obj %p, diskID is %ld, offset %ld, buflength "
	       "%ld\n", this, diskID, offset, buflength);
#endif

    // touch the page to get in page table
    *((uval*)(pageBufAddr)) = 0;
    (void) MamboBogusDiskRead(diskID, (void*) pageBufAddr,
			      offset / BD_SECT_SZ, buflength / BD_SECT_SZ);
    // FIXME: not dealing with any sort of errors ...
    dataRead = buflength;
    memcpy((void *)buf, (void *)pageBufAddr, dataRead);
    return buflength;
}


/* virtual */ SysStatusUval
KernBogusDisk::_writePhys(uval offset, uval paddr)
{
#if 0
    err_printf("In KernBogusDisk::_writePhys for offset %ld\n", offset);
#endif

    passertMsg(PAGE_SIZE <= BD_MAX_BUF, "Not dealing with len > BD_MAX_BUF "
	       "(len is %ld)\n", PAGE_SIZE);
    passertMsg(offset % BD_SECT_SZ == 0, "Not dealing with this type of "
	       "offset yet (objOffset 0x%lx)\n", offset);
    passertMsg(PAGE_SIZE % BD_SECT_SZ == 0, "Not dealing with this len (%ld) yet\n",
	       PAGE_SIZE);

    // not handling errors
    (void) MamboBogusDiskWrite(diskID, (void*) paddr,
			       offset / BD_SECT_SZ, PAGE_SIZE / BD_SECT_SZ);

    return 0;

}

/* virtual */ SysStatusUval
KernBogusDisk::_readPhys(uval offset, uval paddr)
{
#if 0
    err_printf("In KernBogusDisk::_readPhys for offset %ld\n", offset);
#endif

    passertMsg(PAGE_SIZE <= BD_MAX_BUF, "Not dealing with len > BD_MAX_BUF "
	       "(len is %ld)\n", PAGE_SIZE);
    passertMsg(offset % BD_SECT_SZ == 0, "Not dealing with this type of "
	       "offset yet (objOffset 0x%lx)\n", offset);
    passertMsg(PAGE_SIZE % BD_SECT_SZ == 0, "Not dealing with this len (%ld) yet\n",
	       PAGE_SIZE);

    // not handling errors
    (void) MamboBogusDiskRead(diskID, (void*) paddr,
			      offset / BD_SECT_SZ, PAGE_SIZE / BD_SECT_SZ);

    return 0;
}
