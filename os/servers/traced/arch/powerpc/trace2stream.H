#ifndef __POWERPC_TRACE2STREAM_H_
#define __POWERPC_TRACE2STREAM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This stream is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the stream LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: trace2stream.H,v 1.1 2004/04/12 16:59:36 aabauman Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine dependent part of trace2stream.h
 * **************************************************************************/

#include <misc/arch/powerpc/simSupport.H>

SysStatus
trace2streamArchOpen(VPNum vp) {
    return((SysStatus)SimOSSupport(SimTraceFileCtlK, SimTraceFileCtlOpenK, vp));
}

SysStatus
trace2streamArchClose(VPNum vp) {
    return((SysStatus)SimOSSupport(SimTraceFileCtlK,SimTraceFileCtlCloseK, vp));
}

SysStatusUval
trace2streamArchWrite(VPNum vp, uval bufferAddr, uval size) {
    uval64 bufferAddrPhys;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    uval64 *const trcArray = trcInfo->traceArray;

    // bufferAddr is virtual we need to make it physical
    bufferAddrPhys = uval(trcInfo->traceArrayPhys) +
				(bufferAddr - uval(trcArray));

    return((SysStatusUval)SimOSSupport(SimTraceFileCtlK, SimTraceFileCtlWriteK,
				       vp, bufferAddrPhys, size));
}

#endif /* #ifndef __POWERPC_TRACE2STREAM_H_ */
