/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sendfile.C,v 1.11 2004/06/21 03:14:18 apw Exp $
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

#include "FD.H"
#include <io/FileLinux.H>

#define sendfile __k42_linux_sendfile

#include <unistd.h>
#include <sys/sendfile.h>

ssize_t
sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    SysStatus rc;
    SYSCALL_ENTER();
    uval addr;
    uval maxcount;
    uval inOffset;
    ObjectHandle frOH;
    // try to map the input file
    struct stat stat_buf;
    FileLinuxRef inFileRef, outFileRef;

    inOffset = *offset;

    inFileRef =  _FD::GetFD(in_fd);
    if (!inFileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    outFileRef = _FD::GetFD(out_fd);
    if (!outFileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    // no get the object handle to the FR
    rc = DREF(inFileRef)->getFROH(frOH, FileLinux::DEFAULT);
    if (_FAILURE(rc)) {
	//FIXME - do a copy if we can't map the input file
	//        this happens if the input is a stream or other funny
	//        read the man page and remember to reset the in cursor
	SYSCALL_EXIT();
	return -EBADF;
    }

    // find out how long the input file is, incase count is too big
    rc = DREF(inFileRef)->getStatus(
	FileLinux::Stat::FromStruc(&stat_buf));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    maxcount = (uval)stat_buf.st_size-inOffset;
    if (count>maxcount) count = maxcount;

    // always map the whole file. offset might not be page aligned
    rc = StubRegionDefault::_CreateFixedLenExt(
	addr, inOffset+count, 0, frOH, 0,
	AccessMode::readUserReadSup, 0, RegionType::K42Region);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    // now deal with output file

    uval inAddr, inCount;
    inAddr = addr+inOffset;	// read pointer in mapped region
    inCount = count;			// amount left to write

    uval doBlock = ~(DREF(outFileRef)->getFlags()) & O_NONBLOCK;
    ThreadWait *tw = NULL;
    ThreadWait **ptw = NULL;
    if (doBlock) {
	ptw = &tw;
    }
    while (inCount > 0) {
	GenState moreAvail;
	rc=DREF(outFileRef)->write((char *)inAddr, inCount, ptw, moreAvail);

	if (_SUCCESS(rc)) {
	    // wrote something
	    inCount -= _SGETUVAL(rc);
	    inAddr += _SGETUVAL(rc);
	    rc = count;			// incase we are done
	} else if (tw) {
	    while (!tw->unBlocked()) {
		SYSCALL_BLOCK();
#if 0
//FIXME - is sendfile interupptable - I hope not but ...
//        if it is must fix up return values
		if (SYSCALL_SIGNALS_PENDING()) {
		    SYSCALL_EXIT();
		    return -EINTR;
		}
#endif
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    rc = -_SGENCD(rc);
	    goto finished;
	}
    }

finished:
    // clean up region and return
    DREFGOBJ(TheProcessRef)->regionDestroy(addr);
    *offset = (off_t)(inAddr - addr);
    SYSCALL_EXIT();
    return rc;
}

extern "C" sval32
__k42_linux_sendfile_32(sval32 out_fd, sval32 in_fd,
			sval32 *offset, uval32 count)
{
    sval32 ret;
    off_t offset64 = SIGN_EXT(*offset);

    ret = __k42_linux_sendfile(SIGN_EXT(out_fd), SIGN_EXT(in_fd),
			       &offset64, SIGN_EXT(count));

    *offset = offset64;

    return ret;
}
