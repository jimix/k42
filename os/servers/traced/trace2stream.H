#ifndef __TRACE2STREAM_H_
#define __TRACE2STREAM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: trace2stream.H,v 1.5 2004/12/15 19:58:53 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides simple test interface to a user program
 * **************************************************************************/

#include <io/IO.H>
#include __MINC(trace2stream.H)

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

SysStatus
trace2diskOpen(VPNum physProc);

SysStatus
trace2streamOpen(VPNum physProc) {
    SysStatus rc;
    char filename[PATH_MAX];

    //if (KernelInfo::OnSim()) return (trace2streamArchOpen(physProc));
    if (traceToNetwork) return 0;
    if (traceToDisk) return 0;

    snprintf(filename, sizeof(filename), "%s/%s.%ld.trc",
             outPathName, baseFileName, physProc);

    rc = FileLinux::Create(traceFileRef[physProc], filename,
			   O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed rc %lx\n", filename, rc);
	return (rc);
    }
    return rc;
}

SysStatus
trace2streamClose(VPNum physProc) {
    //if (KernelInfo::OnSim()) return (trace2streamArchClose(physProc));
    if (traceToNetwork) return 0;
    if (traceToDisk) return 0;
    return DREF(traceFileRef[physProc])->destroy();
}

#include <endian.h>
#include <byteswap.h>

#if __BYTE_ORDER == __BIG_ENDIAN
# define hton64(x) (x)
#else
# define hton64(x) bswap_64(x)
#endif

#define MAX_PAYLOAD	1400

static SysStatusUval
trace2netWrite(VPNum physProc, uval bufferAddr, uval size)
{
    ssize_t totlen = 0, len;
    struct msghdr msghdr;
    struct iovec iovecs[2];
    uint64_t seqnum;
    char sendbuf[MAX_PAYLOAD]; /* REMOVEME: see comment on the memcpy below */

    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_iov = iovecs;
    msghdr.msg_iovlen = 2;

    /* FIXME: this shouldn't be needed, since we connect() at init time */
    msghdr.msg_name = &traceNetwork[physProc].dest;
    msghdr.msg_namelen = sizeof(traceNetwork[physProc].dest);

    iovecs[0].iov_base = &seqnum;
    iovecs[0].iov_len = sizeof(seqnum);

    while (size > 0) {
	seqnum = hton64(traceNetwork[physProc].seqnum);
	iovecs[1].iov_len = (size > MAX_PAYLOAD) ? MAX_PAYLOAD : size;
#if 0
	/* this doesn't work because the K42 VM isn't set up to handle
	 * Pin requests on the trace region, so instead we have to use
	 * the copy below, until it gets fixed */
	iovecs[1].iov_base = (void *)bufferAddr;
#else
	iovecs[1].iov_base = sendbuf;
	memcpy(sendbuf, (void *)bufferAddr, iovecs[1].iov_len);
#endif
	bufferAddr += iovecs[1].iov_len;
	size -= iovecs[1].iov_len;
	traceNetwork[physProc].seqnum += iovecs[1].iov_len;

	len = sendmsg(traceNetwork[physProc].sockfd, &msghdr, 0/*MSG_CONFIRM*/);
	if (len <= 0) {
	    err_printf("traced: Error: sendmsg on VP %lu: %s\n", physProc,
		       strerror(errno));
	    return -1; /* FIXME: should use K42-ish error code */
	} else {
	   totlen += len - sizeof(seqnum);
	}
    }

    return _SRETUVAL(totlen);
}

SysStatusUval
trace2streamWrite(VPNum physProc, uval bufferAddr, uval size) {
    SysStatusUval rc;
    SysStatus rcf;
    //if (KernelInfo::OnSim()) {
    //	return (trace2streamArchWrite(physProc, bufferAddr, size));
    //} else
    if (traceToNetwork) {
	return trace2netWrite(physProc, bufferAddr, size);
    }
    if (traceToDisk) {
      rc = diskTrace->write(physProc, bufferAddr, size);
      tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
      return rc;
    }

    while (1) {
	GenState moreAvail;
	ThreadWait *tw = NULL;
	rc = DREF(traceFileRef[physProc])->write((const char *)bufferAddr,
						 size, &tw, moreAvail);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    rcf = DREF(traceFileRef[physProc])->flush();
	    tassertWrn(_SUCCESS(rcf), "traced flush failed\n");
	    break;
	}
    }

    if (_FAILURE(rc)) {
	cprintf("write of bufferaddr %ld size %ld failed\n", bufferAddr, size);
    }
    rcf = DREF(traceFileRef[physProc])->flush();
    _IF_FAILURE_RET(rcf);
    return rc;
}

#endif /* #ifndef __TRACE2STREAM_H_ */
