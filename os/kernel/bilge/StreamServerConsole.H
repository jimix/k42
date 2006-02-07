#ifndef __STREAM_SERVER_CONSOLE_H_
#define __STREAM_SERVER_CONSOLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerConsole.H,v 1.20 2005/07/15 17:14:28 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports interface to console to external
 * programs. Most of this is so that we can access a console as a pty,
 * there is a single static call for writes to console as a console.
 * **************************************************************************/
#include <io/StreamServer.H>
#ifndef _IN_STUBGEN
#include <meta/MetaStreamServer.H>
#include <meta/MetaStreamServer.H>
#include <stub/StubFileLinuxClientStream.H>
#include <cobj/XHandleTrans.H>
#include <io/FileLinux.H>
#include <io/IORingBuffer.H>
#endif


class StreamServerConsole : public StreamServer __xbase(Obj) {
  protected:
    IORingBuffer buffer;
    static StreamServerConsole* con;
    XHandle login;
    virtual SysStatus handleXObjFree(XHandle xhandle);
    StreamServerConsole(): login(0) {};
    DEFINE_ACCESS_SERVER(StreamServer,
			 MetaStreamServer::write|MetaStreamServer::read|
			 MetaStreamServer::controlAccess,
			 MetaStreamServer::write|MetaStreamServer::read|
			 MetaStreamServer::controlAccess);

    DEFINE_PINNEDGLOBALPADDED_NEW(StreamServerConsole);

    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData* client=NULL);
  public:
    virtual SysStatus detach() { return 0; }

    // should only be on client side
    virtual SysStatus dup(IORef& newIORef) {
	return _SERROR(1554, 0, EINVAL);
    }
    virtual SysStatus flush() { return 0; };
    virtual SysStatus destroy() { buffer.destroy(); return 0; };
    virtual SysStatusUval recvfrom(struct iovec *vec, uval veclen, uval flags,
				   char *addr, uval &addrLen,
				   GenState &moreAvail, 
				   void *controlData, uval &controlDataLen,
				   __XHANDLE xhandle);
    virtual SysStatusUval sendto(struct iovec* vec, uval veclen, uval flags,
				 const char *addr, uval addrLen,
				 GenState &moreAvail, 
				 void *controlData, uval controlDataLen,
				 __XHANDLE xhandle);

    /* end exported functions */
    static void Init1(VPNum vp);
    static void Init2(VPNum vp);
    static void StartLoginShell(ObjectHandle &oh, ProcessID pid);
    virtual void startLoginShell(ObjectHandle &oh, ProcessID pid);
    virtual SysStatus kosherxlist();
  protected:
    virtual void readFromConsole();	// thread just sits there reading
    static void ReadFromConsole(uval t); // reads from real console

};
#endif /* #ifndef __STREAM_SERVER_CONSOLE_H_ */
