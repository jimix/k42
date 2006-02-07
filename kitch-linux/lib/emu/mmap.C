/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmap.C,v 1.43 2005/04/20 20:26:56 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include "linuxEmul.H"
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <sys/stat.h>
#include <trace/traceMem.h>

#include "FD.H"
#include <io/FileLinux.H>

#define mmap __k42_linux_mmap
#define mmap2 __k42_linux_mmap2
#define munmap __k42_linux_munmap
#define mprotect __k42_linux_mprotect

extern "C" void* mmap2(void* addr, size_t len, unsigned long prot,
		       unsigned long flags, unsigned long fd,
		       unsigned long pgoff);
#include <unistd.h>
#include <sys/mman.h>

// temp kludge for testing - use offset as page size
#define MAP_LARGEPAGE 0x1000000
#define GPUL_LARGE_PAGE_SIZE 0x1000000

// taken from glibc malloc.c since we are basing our hacked version on
// glibc's malloc requirements.
#ifndef MAP_NORESERVE
# ifdef MAP_AUTORESRV
#  define MAP_NORESERVE MAP_AUTORESRV
# else
#  define MAP_NORESERVE 0
# endif
#endif

// only in x86_64 tree - don't know why not in PPC64
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif

/*
 *FIXME malloc kludge - until munmap and mprotect work for
 *      computational regions, we have a total kludge.
 *      malloc tries to mmap a large region, protected.
 *      it then unmap the beginning and end to get a HEAP_MAX_SIZE
 *      alligned HEAP_MAX_SIZE sized chunk.  Finally, it allows
 *      access only to size of this chunk.  Later, it grows the heap
 *      by further mprotect calls to enable more of it.
 *      Our kludge is to map the whole thing read/write, and then
 *      ignore the munmap and mprotect calls.
 *
 *      A new usage is seen - mmap a large region protected, then
 *      mmap the beginning again to allow access.
 *      We deal with this by just making believe these mappings worked!
 *      I really need to understand mmap semantics!
 *N.B. see munmap kludge below as well
 *
 *      ld.so kludge
 *      ld.so mmap's file with length enough for code and data at correct
 *      virtual addresses relative to start.  It then mprotects none
 *      everything but the beginning up to end of text.  It then
 *      mmap's data and bss on top of the mprotected part of the initial
 *      mapping.  It does this so it can reserve the correct ammount of
 *      contiguous space.  The mprotect is relegious - it prevents erroneous
 *      accesses to unused areas of the original big mapping from seeing
 *      accidental values.  But it gets in the way of using a shared
 *      segment mapping for the text.  For now, we ignore mprotect
 *      and kludge the map over operations.  In theory, when data is mapped,
 *      the area following it should still be mapped by the initial mapping,
 *      But thats a lot of work, to be followed immediately by mapping bss.
 *      So we truncate the initial region to make room for data, instead
 *      of splitting it.
 *      N.B. all this is garbage - getting mmap anywhere near right efficiently
 *      is hard - and may need lazy mapping ops or a very different kernel
 *      mechanism than we have now.  And the mprotect in ld.so is bad for
 *      anyone who does shared segments with data/bss in a different segment
 *      from text.
 */

static uval mmap_oh(
    uval start, size_t length, int prot, int flags,
    off_t offset, ObjectHandle frOH, RegionType::Type regionType)
{
    SysStatus rc;
    AccessMode::mode access;

    // FIXME: This is part of the malloc kludge described above.
    if (flags & MAP_ANONYMOUS) {
	prot |= (PROT_READ | PROT_WRITE);
    }

    if (prot & PROT_WRITE) {
	access = AccessMode::writeUserWriteSup;
    } else if (prot & PROT_READ) {
	access = AccessMode::readUserReadSup;
    } else {
	access = AccessMode::noUserWriteSup;
    }

    if (prot & PROT_EXEC) {
	access = (AccessMode::mode)(access | AccessMode::execute);
    }

    rc = -1;			// used to see if first try worked
    if (flags & MAP_FIXED) {
	if (start == 0 ) {
	    return uval(-EINVAL);
	}
	// map fixed is defined to overwrite an existing mapping
	// see if there is one

	RegionType::RegionElementInfo element;
	rc = DREFGOBJ(TheProcessRef)->findRegion(start, element);
	if (_SUCCESS(rc)) {
	    if (element.type != RegionType::ForkCopy &&
	     element.type != RegionType::UseSame) {
		return uval(-EINVAL);
	    }
	    uval trunclength;
	    //FIXME truncate the rest - we can't make holes yet
	    trunclength = element.start + element.size - start;
	    rc = DREFGOBJ(TheProcessRef)->regionTruncate(start, trunclength);
	    tassertMsg(_SUCCESS(rc),"cant fail but got rc %lx", rc);
	}
    }

    if (start) {
#if 0
	{
            #include <misc/linkage.H>
	    err_printf("marc needs this traceback unless its from regress\n");
	    uval callchain[20], i;
	    GetCallChainSelf(0, callchain, 20);
	    for (i=0; i<20 && callchain[i]; i++) {
		err_printf("%lx ", callchain[i]);
		if ((i&3) == 3) err_printf("\n");
	    }
	    err_printf("\n");
	}
#endif
	// try fixed address
	rc = StubRegionDefault::_CreateFixedAddrLenExt(
	    start, length, frOH, offset,
	    access, 0, regionType);
	if (_FAILURE(rc) && (flags & MAP_FIXED)) {
	    //fixme - really need to know why the attach failed
	    return uval(-ENOMEM);
	}
    }

    // if we never tried above, or if it failed, try any address
    if (_FAILURE(rc)) {
	if (flags & MAP_32BIT) {
	    start = PAGE_SIZE;		// skip first page
	    // mman page says 2gig but allow 4 for powerpc?
	    rc = StubRegionDefault::_CreateFixedLenWithinRangeExt(
		start, 0x100000000, length, 0, frOH, offset,
		access, 0, regionType);
	} else {
	    rc = StubRegionDefault::_CreateFixedLenExt(
		start, length, 0, frOH, offset,
		access, 0, regionType);
	}
	tassertWrn(_SUCCESS(rc),"mmap failed: %lx %lx\n",rc,length);
    }

    if (_FAILURE(rc)) {
	//fixme - really need to know why the attach failed
	return uval(-ENOMEM);
    }

    return start;
}

void *
mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    SysStatus rc;
    SYSCALL_ENTER();
    uval start;

    ObjectHandle frOH;

//    err_printf("mmap(start %p, length %lx, prot %x, flags %x, "
//	       "fd %d, offset %lx)\n",
//	       start, length, prot, flags, fd, offset);

    start = (uval)addr;
    length = ALIGN_UP(length, PAGE_SIZE);

    if ((flags & (MAP_PRIVATE|MAP_SHARED)) == 0) {
	    SYSCALL_EXIT();
	    return (void *) -EINVAL;
    }

    RegionType::Type regionType;
    regionType = (flags&MAP_SHARED)?RegionType::UseSame:RegionType::ForkCopy;

    /*
     * we will be adding mmap support as needed.  anonymous and file mapping
     * are the two firsts
     */
    if (flags & MAP_ANONYMOUS) {
	// first see if its a mapping we already have - if so make
	// believe it worked
	if (flags & MAP_FIXED) {
	    RegionType::RegionElementInfo element;
	    rc = DREFGOBJ(TheProcessRef)->findRegion(start, element);
	    
	    if (_SUCCESS(rc) &&
		element.start <= start &&
		(element.start+element.size)>=(start+length) &&
		element.type == regionType) {
		/*
		 * FIXME see malloc kludge above.  For now, silently ignore
		 * operations on forkCopy regions.
		 */
		SYSCALL_EXIT();
		return (void *) start;
	    }
	}

	// currently the interface for mmap using large pages is to pass the
	// size of the requested size in the offset parameter, as we support
	// more page sizes the passert will need to be updated or if the model
	// for how to request large pages changes
	if(flags & MAP_LARGEPAGE) {
	    passertMsg((offset == GPUL_LARGE_PAGE_SIZE),
		       "Offset of %ld does not have large page value 0x%llx", 
		       offset, (uval64)GPUL_LARGE_PAGE_SIZE);
	    rc = StubFRComputation::_CreateLargePage(frOH, offset);
	} else {
	    rc = StubFRComputation::_Create(frOH);
	}

	if (_FAILURE(rc)) {
	    //FIXME is this the right errno
	    SYSCALL_EXIT();
	    return (void *) -ENOMEM;
	}

	start = mmap_oh(start, length, prot, flags,
		        0, frOH, regionType);

	TraceOSMemMMap((uval64)start, (uval64)length, (uval64)flags);

	// don't need the object handle any more
	Obj::ReleaseAccess(frOH);

	SYSCALL_EXIT();
	return (void *) start;
    } else {
	// map a file
	struct stat stat_buf;
	FileLinuxRef fileRef;
	fileRef =  _FD::GetFD(fd);
	if (!fileRef) {
	    SYSCALL_EXIT();
	    return (void *) -EBADF;
	}
	rc = DREF(fileRef)->getFROH(
	    frOH, flags&MAP_SHARED?FileLinux::DEFAULT:FileLinux::COPYONWRITE);
	if (_FAILURE(rc)) {
	    SYSCALL_EXIT();
	    return (void *) -EBADF;
	}

	rc = DREF(fileRef)->getStatus(
	    FileLinux::Stat::FromStruc(&stat_buf));
	if (_FAILURE(rc)) {
	    SYSCALL_EXIT();
	    return (void *)-_SGENCD(rc);
	}

/*
 *FIXME should we need to grow file, or should it grow by itself
 */
	if (prot & PROT_WRITE && (size_t)stat_buf.st_size < length) {
	    rc = DREF(fileRef)->ftruncate(length);
	    if (_FAILURE(rc)) {
		SYSCALL_EXIT();
		return (void *) -_SGENCD(rc);
	    }
	}

	if (flags & MAP_PRIVATE) {
	    //FIXME debugging kludge
	    //if PROT_EXEC is set, make writable so we can debug
	    if (prot & PROT_EXEC) prot |= PROT_WRITE;
	}

	start = mmap_oh(start, length, prot, flags,
			offset, frOH, regionType);

	Obj::ReleaseAccess(frOH);

	SYSCALL_EXIT();
	return (void *) start;
    }

    SYSCALL_EXIT();
    return ((void *) __k42_linux_emulNoSupport (__PRETTY_FUNCTION__, ENOSYS));
}

extern "C" void *
__k42_linux_mmap_32(void *addr, size_t length, int prot, int flags,
                    int fd, off_t offset)
{
    flags |= MAP_32BIT;
    return mmap(addr, length, prot, flags, fd, offset);
}

void*
mmap2(void* addr, size_t len,
      unsigned long prot, unsigned long flags,
      unsigned long fd, unsigned long pgoff)
{
    return mmap(addr, len, prot, flags, fd, pgoff * PAGE_SIZE);
}

extern "C" void *
__k42_linux_sys32_mmap2(void *addr, size_t length, int prot, int flags,
			int fd, off_t pgoff)
{
    flags |= MAP_32BIT;
    return mmap2(addr, length, prot, flags, fd, pgoff);
}

int
munmap(void *addr, size_t length)
{
    SysStatus rc;
    SYSCALL_ENTER();
    uval start = (uval)addr;

    RegionType::RegionElementInfo element;
    rc = DREFGOBJ(TheProcessRef)->findRegion(start, element);

    // for now only remove exactly a complete mapping
    if (_FAILURE(rc) ||
       element.start != start || element.size != length ||
       (element.type != RegionType::ForkCopy &&
	element.type != RegionType::UseSame)) {
	/*
	 * FIXME see malloc kludge above.  For now, silently ignore
	 * operations on forkCopy regions.
	 */
	if (_SUCCESS(rc) && element.type == RegionType::ForkCopy) {
	    SYSCALL_EXIT();
	    return 0;
	}
	SYSCALL_EXIT();
	return -EINVAL;
    }

    TraceOSMemMUnmap((uval64)start, (uval64)length);

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(start);
    tassertMsg(_SUCCESS(rc),"cant fail but got rc %lx", rc);

    SYSCALL_EXIT();
    return 0;
}

int
mprotect(void *addr, uval length, int prot)
{
    uval start;
    SYSCALL_ENTER();

    start = (uval)addr;

    length = ALIGN_UP(length, PAGE_SIZE);

    /*
     * mmap() ignores the protection parameter for anonymous mappings, so we
     * can hope the region we're called on is already read-write.
     */
    if (prot == (PROT_READ|PROT_WRITE)) {
#if 0
	err_printf("mprotect assuming start %p, len %lx "
		   "is already read-write.\n",
		   start, len);
#endif
	SYSCALL_EXIT();
	return 0;
    } else if (prot == PROT_NONE) {
// see ld.so kludge above
#if 0
	uval start = (uval)start;

	RegionType::RegionElementInfo element;
	rc = DREFGOBJ(TheProcessRef)->findRegion(start, element);
	if (_FAILURE(rc) ||
	    (element.start != start &&
	     (element.start + element.size != start + length)) ||
	    (element.type != RegionType::ForkCopy &&
	     element.type != RegionType::UseSame)) {
	    SYSCALL_EXIT();
	    return -EINVAL;
	}
	if ((start == element.start) && (length == element.size)) {
	    SYSCALL_EXIT();
	    return munmap(start, length);
	}
	DREFGOBJ(TheProcessRef)->regionTruncate(start, length);
#endif
	SYSCALL_EXIT();
	return 0;
    } else {
	SYSCALL_EXIT();
	#undef VERBOSE_MPROTECT
	#ifdef VERBOSE_MPROTECT
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    tassertWrn(0, "mprotect ignored prot 0x%x\n addr %p size 0x%lx\n",
		   prot, addr, length);
	}
	#endif // VERBOSE_MPROTECT
	return 0;
//	return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, ENOSYS));
    }
}

