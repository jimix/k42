#ifndef __SIMIP_H_
#define __SIMIP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simip.h,v 1.45 2004/09/29 08:32:53 cyeoh Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines interface for IP cut-through over
 * thinwire. It would be nice if we could adopt the Tornado approach
 * of having everything use IP.  Unfortunately, running on x86
 * platforms we cannot always do this, since we will not have an NFS
 * server.  Hence, we have to support many more operations here, e.g.,
 * open, fstat...
 *
 * **************************************************************************/

/* I put here all the routines supported on Tornado, some of these are
 * not needed for IP, but are needed in a cut-through, so put them
 * here.  All requests come from the k42, as a single byte
 * request type, of one of the following, and then a struct containing
 * data.  The k42 side is responsible for doing select
 * operations to make sure that data is available on a connection
 * before doing a blocking operation on that connection, or, wherever
 * possible, doing non-blocking operations. The non-k42 packets
 * are always as part of an exepected response */
enum {
    SIMIP_SOCKET=1,
    SIMIP_CLOSE,
    SIMIP_BIND,
    SIMIP_LISTEN,
    SIMIP_ACCEPT,
    SIMIP_READ,
    SIMIP_WRITE,
    SIMIP_SENDTO,
    SIMIP_RECVFROM,
    SIMIP_CONNECT,
    SIMIP_GETENVVAR,
    SIMIP_GETTIMEOFDAY,
    SIMIP_GETKPARM_BLOCK,

    // REST NYI
    SIMIP_RECV,
    SIMIP_SEND,
    SIMIP_IOCTL_FIONREAD,
    SIMIP_IOCTL_FIONBIO,
    SIMIP_IOCTL_FL,
    SIMIP_SETSOCKOPT,

    SIMIP_FCNTL_NONBLOCK,
    SIMIP_GET_SOCKNAME,
    SIMIP_GET_PEERNAME
};

#ifndef __TYPES_H_ // k42 types.H should be included first
typedef unsigned int		uval32;
typedef unsigned short		uval16;
typedef short			sval16;
typedef int			sval32;
typedef unsigned char           uval8;
#endif /* #ifndef __TYPES_H_ // k42 types.H ... */

struct simipSocketRequest {
    uval32 type;
};

struct simipSocketResponse {
    sval32 sock;
};

struct simipCloseRequest {
    sval32 sock;
};

struct simipCloseResponse {
    sval32 rc;
    uval32 errnum;			// if rc == -1, has unix errno
};

struct simipBindRequest {
    sval32 sock;
    uval32 port;
    uval32 addr;
};

struct simipBindResponse {
    sval32 rc;
    sval32 errnum;			// if rc == -1
};

struct simipListenRequest {
    sval32 sock;
    sval32 backlog;
};

struct simipListenResponse {
    sval32 rc;
    sval32 errnum;			// if rc == -1
};

struct simipAcceptRequest {
    sval32 sock;
};

struct simipAcceptResponse {
    sval32 rc;			        // if positive, is result
    uval32 block;			// if success == -1, says nothing avail
    uval32 available;			// more connections available
    sval32 errnum;			// if rc == -1
};

struct simipReadRequest {
    sval32 sock;
    uval32 nbytes;
};

struct simipReadResponse {
    sval32 nbytes;			// -1 if there is an error
    uval32 available;
    uval32 block;
    sval32 errnum;
};

struct simipWriteRequest {
    sval32 sock;
    uval32 nbytes;
};

struct simipWriteResponse {
    sval32 nbytes;			// -1 if there is an error
    sval32 errnum;
};

struct simipSendtoRequest {
    sval32 sock;
    uval32 nbytes;
    uval32 port;
    uval32 addr;
};

struct simipSendtoResponse {
    sval32 nbytes;			// -1 if there is an error
    sval32 errnum;
};

struct simipRecvfromRequest {
    sval32 sock;
    uval32 nbytes;
};

struct simipRecvfromResponse {
    sval32 nbytes;			// -1 if there is an error
    uval32 available;
    uval32 block;
    uval32 port;
    uval32 addr;
    sval32 errnum;
};

struct simipConnectRequest {
    sval32 sock;
    uval32 port;
    uval32 addr;
};

struct simipConnectResponse {
    sval32 rc;
    sval32 errnum;			// if rc == -1
};

//Yes, these are big buffers but we don't use this alot
struct simipGetEnvVarRequest {
    char envVarName[128];
};

struct simipGetEnvVarResponse {
    char envVarValue[256];
};

struct simipGetTimeOfDayResponse {
    uval32 tv_sec, tv_usec;
};

#endif /* #ifndef __SIMIP_H_ */
