#ifndef __GENERIC64_TRACE2STREAM_H_
#define __GENERIC64_TRACE2STREAM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
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


SysStatus
trace2streamArchOpen(VPNum physProc) {
  return(0);
}

SysStatus
trace2streamArchClose(VPNum physProc) {
  return(0);
}

SysStatusUval
trace2streamArchWrite(VPNum physProc, uval bufferAddr, uval size) {
  return(0);
}

#endif /* #ifndef __GENERIC64_TRACE2STREAM_H_ */
