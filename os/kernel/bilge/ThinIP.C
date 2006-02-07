/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ThinIP.C,v 1.111 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "ThinIP.H"
#include "ThinWireMgr.H"
#include <sys/simip.h>
#include <io/PathName.H>
#include <limits.h>
#include <misc/hardware.H>
#include <scheduler/Scheduler.H>
#include <sys/time.h>
#include <sys/KernelInfo.H>                // for ::OnSim()
#include <misc/arch/powerpc/simSupport.H>  // for MamboGetEnv()
#include <bilge/arch/powerpc/BootInfo.H>   // for SIM_MAMBO
#include "defines/sim_bugs.H"
#if defined(TARGET_mips64)
#include __MINC(GizmoIP.H)
#endif /* #if defined(TARGET_mips64) */

ThinIP *ThinIP::obj = NULL;

/* static */ void
ThinIP::BeAsynchronous(uval arg)
{
    while (1) {
	char c;
	sval i;
	StreamServerRef sockRef;

	Scheduler::DeactivateSelf();	// remove for garbage collection
	i = obj->select_chan->read(&c, 1, 1);
	Scheduler::ActivateSelf();	// re-instate

	if (i != 1) {
	    tassertWrn(0, "ThinIP down? Didn't get a single character: %ld\n",
		       i);
	    return;
	}

	obj->lock.acquire();
	sockRef = obj->sockets[int(c)];
	obj->lock.release();

	if (sockRef != NULL) {
	    DREF(sockRef)->signalDataAvailable();
	} else {
	    tassertWrn(0, "Ignoring data available on bad socket.\n");
	}
    }
}

void
ThinIP::ClassInit(VPNum vp, IOChan* ipcut, IOChan* ipsel)
{
    if (vp==0) {
	if (vp!=0) return;			// nothing to do proc two
#if defined(TARGET_powerpc)
	obj = new ThinIP();
#elif defined(TARGET_mips64)
	obj = new GizmoIP();
	//obj = new ThinIP();
#elif defined(TARGET_amd64)
	obj = new ThinIP();			// XXX pdb
#elif defined(TARGET_generic64)
	obj = new ThinIP();
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
	tassert(obj != NULL, err_printf("oops\n"));
	obj->init(ipcut, ipsel);
    }
}


void
ThinIP::init(IOChan* ipcut, IOChan* ipsel)
{
    select_chan = ipsel;
    cut_chan = ipcut;
    obj->lock.init();
    for (uval i=0;i<MAX_SOCKETS;i++) {
	obj->sockets[i] = 0;
    }
#ifndef FAST_REGRESS_ON_SIM
    SysStatus rc = Scheduler::ScheduleFunction(BeAsynchronous, 0, obj->daemon);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
#endif
}


sval
ThinIP::locked_blockedRead(void *p, int length)
{
    _ASSERT_HELD(lock);

    sval i;
    while (length > 0) {
	i = cut_chan->read((char*)p, length, 1);
	if (_FAILURE(i)) {
	    return i;
	}
	length -= i;
	p = (void *)(((unsigned long)p) + i);
    }
    return 0;
}

sval
ThinIP::locked_blockedWrite(const void *p, int length)
{
    _ASSERT_HELD(lock);

    return cut_chan->write((char*)p, length, 1);
}

SysStatus
ThinIP::socket(sval32 &sock, StreamServerRef ref, uval type)
{
    struct simipSocketRequest out;
    struct simipSocketResponse in;
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_SOCKET;
    locked_blockedWrite(&c, 1);

    out.type = type;
    locked_blockedWrite(&out, sizeof(out));

    rc = locked_blockedRead(&in, sizeof(in));
    if (_FAILURE(rc)) {
	return rc;
    }
    sock = in.sock;
    tassert((sock<MAX_SOCKETS), err_printf("woops\n"));
    sockets[sock] = ref;
    return 0;
}

SysStatus
ThinIP::close(sval32 sock)
{
    struct simipCloseRequest in;
    struct simipCloseResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_CLOSE;
    locked_blockedWrite(&c, 1);

    in.sock = sock;

    locked_blockedWrite(&in, sizeof(in));
    locked_blockedRead(&out, sizeof(out));
    if (out.rc == 0) {
	return 0;
    }

    return _SERROR(1263, 0, out.errnum);
}

SysStatus
ThinIP::bind(sval32 sock, void* addr, uval addrLen)
{
    struct sockaddr_in *sin = (struct sockaddr_in*)addr;
    struct simipBindRequest in;
    struct simipBindResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_BIND;
    locked_blockedWrite(&c, 1);

    in.sock = sock;
    in.port = sin->sin_port;
    in.addr = sin->sin_addr.s_addr;
    locked_blockedWrite(&in, sizeof(in));
    locked_blockedRead(&out, sizeof(out));
    if (out.rc < 0) {
	return _SERROR(1259, 0, out.errnum);
    }

    return 0;
}

SysStatus
ThinIP::listen(sval32 sock, sval32 backlog)
{
    struct simipListenRequest in;
    struct simipListenResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_LISTEN;
    locked_blockedWrite(&c, 1);

    in.sock = sock;
    in.backlog = backlog;
    locked_blockedWrite(&in, sizeof(in));

    locked_blockedRead(&out, sizeof(out));
    if (out.rc<0)
	return _SERROR(1260, 0, out.errnum);

    return 0;
}

SysStatus
ThinIP::accept(sval32 sock, sval32 &clientSock, StreamServerRef ref,
	       uval &available)
{
    struct simipAcceptRequest in;
    struct simipAcceptResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_ACCEPT;
    locked_blockedWrite(&c, 1);

    in.sock = sock;
    locked_blockedWrite(&in, sizeof(in));
    locked_blockedRead(&out, sizeof(out));

    if (out.rc<0) {
	if (out.block) {
	    return _SERROR(1261, WOULD_BLOCK, out.errnum);
	} else {
	    return _SERROR(1264, 0, out.errnum);
	}
    }

    clientSock = out.rc;
    available = out.available;

    tassert((clientSock<MAX_SOCKETS), err_printf("woops\n"));
    sockets[clientSock] = ref;
    return 0;
}

SysStatusUval
ThinIP::read(sval32 sock, char *buf, uval32 nbytes, uval &available)
{
    struct simipReadRequest in;
    struct simipReadResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_READ;
    locked_blockedWrite(&c, 1);

    in.sock   = sock;
    in.nbytes = nbytes;
    locked_blockedWrite(&in, sizeof(in));

    locked_blockedRead(&out, sizeof(out));
    if (out.nbytes < 0) {
	SysStatus rc = _SERROR(1536, 0, out.errnum);
	return rc;
    }
    available = out.available;

    locked_blockedRead(buf, out.nbytes);
    return out.nbytes;
}

SysStatusUval
ThinIP::write(sval32 sock, const char *buf, uval32 nbytes)
{
    struct simipWriteRequest in;
    struct simipWriteResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_WRITE;
    locked_blockedWrite(&c, 1);

    in.sock   = sock;
    in.nbytes = nbytes;
    locked_blockedWrite(&in, sizeof(in));
    locked_blockedWrite(buf, in.nbytes);
    locked_blockedRead(&out, sizeof(out));
    if (out.nbytes < 0) {
	SysStatus rc = _SERROR(1537, 0, out.errnum);
	return rc;
    }
    nbytes = out.nbytes;
    return nbytes;
}

SysStatusUval
ThinIP::sendto(sval32 sock, const char *buf, uval32 nbytes,
	       void* addr, uval addrLen)
{
    struct sockaddr_in *sin = (struct sockaddr_in*)addr;
    struct simipSendtoRequest out;
    struct simipSendtoResponse in;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_SENDTO;
    locked_blockedWrite(&c, 1);

    out.sock   = sock;
    out.nbytes = nbytes;
    if (sin) {
	out.port   = sin->sin_port;
	out.addr   = sin->sin_addr.s_addr;
    } else {
	// these values should get ignored since the socket is bound
	out.port   = 0;
	out.addr   = 0;
    }

    locked_blockedWrite(&out, sizeof(out));
    locked_blockedWrite(buf, out.nbytes);
    locked_blockedRead(&in, sizeof(in));
    if (in.nbytes < 0) {
	SysStatus rc = _SERROR(1258, 0, in.errnum);
	return rc;
    }
    nbytes = in.nbytes;
    return nbytes;
}

SysStatusUval
ThinIP::recvfrom(sval32 sock, char *buf, uval32 nbytes,
		 uval &available, void* addr, uval addrLen)
{
    struct simipRecvfromRequest in;
    struct simipRecvfromResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_RECVFROM;
    locked_blockedWrite(&c, 1);

    in.sock   = sock;
    in.nbytes = nbytes;
    locked_blockedWrite(&in, sizeof(in));

    locked_blockedRead(&out, sizeof(out));
    if (out.nbytes < 0) {
	SysStatus rc = _SERROR(1255, 0, out.errnum);
	return rc;
    }

    available = out.available;

    struct sockaddr_in *sin = (struct sockaddr_in*)addr;

    if (sin) {
	sin->sin_family = AF_INET;
	sin->sin_port = out.port;
	sin->sin_addr.s_addr  = out.addr;
	addrLen = sizeof(struct sockaddr_in);
    }

    locked_blockedRead(buf, out.nbytes);
    return out.nbytes;
}

SysStatus
ThinIP::connect(sval32 sock, void* addr, uval addrLen)
{
    struct sockaddr_in *sin = (struct sockaddr_in*)addr;

    tassert(sin->sin_family == AF_INET,
	    err_printf("unsupported addr family\n"));

    struct simipConnectRequest in;
    struct simipConnectResponse out;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_CONNECT;
    locked_blockedWrite(&c, 1);

    in.sock = sock;
    in.port = sin->sin_port;
    in.addr = sin->sin_addr.s_addr;
    locked_blockedWrite(&in, sizeof(in));
    locked_blockedRead(&out, sizeof(out));
    if (out.rc<0)
	return _SERROR(1538, 0, out.errnum);

    return 0;
}

SysStatus
ThinIP::getThinEnvVar(const char *envVarName, char *envVarValue)
{
    SysStatus rc;
    struct simipGetEnvVarRequest out;
    struct simipGetEnvVarResponse in;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_GETENVVAR;

    if ((rc = locked_blockedWrite(&c, 1)) < 0) {
        return rc;
    }

    strcpy(out.envVarName, envVarName);
    locked_blockedWrite(&out, sizeof(out));

    locked_blockedRead(&in, sizeof(in));
    strcpy(envVarValue, in.envVarValue);

    return 0;
}

SysStatus
ThinIP::getThinTimeOfDay(struct timeval& tv)
{
#ifndef FAST_REGRESS_ON_SIM
    struct simipGetTimeOfDayResponse in;
    sval rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_GETTIMEOFDAY;
    rc = locked_blockedWrite(&c, 1);

    tassertMsg(_SUCCESS(rc), "Failed to get time of day from thinwire");

    /* we have to transmit the values as uval32 because the
     * actually size of times may be different here and in
     * the simip program on the host - so we can't just pass
     * the struct timeval around.
     */
    locked_blockedRead(&in, sizeof(in));
    tv.tv_sec = in.tv_sec;
    tv.tv_usec = in.tv_usec;

    return 0;
#else
    tv.tv_sec = 99;
    tv.tv_usec = 17;
    return 0;
#endif
}

SysStatus
ThinIP::getKParms(void **data)
{
    uval32 dataSize;
    sval rc;

    tassertMsg(KernelInfo::OnSim()!=SIM_MAMBO,
	       "Attempt to get kernel parameters from thinwire on mambo");

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    char c = SIMIP_GETKPARM_BLOCK;
    rc = locked_blockedWrite(&c, 1);
    _IF_FAILURE_RET(rc);

    /* First uval32 is size of following data block blob */
    locked_blockedRead(&dataSize, sizeof(dataSize));
    err_printf("Data block size (thinwire): %u\n", dataSize);

    *data = allocGlobal(dataSize);

    locked_blockedRead(*data, dataSize);

    return 0;
}


void
ThinIP::doPolling()
{
    ThinWireMgr::DoPolling();
    Scheduler::Yield();		// have to yeild to give a chance to change
}
