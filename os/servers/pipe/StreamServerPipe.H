#ifndef __STREAM_SERVER_PIPE_H_
#define __STREAM_SERVER_PIPE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerPipe.H,v 1.34 2005/07/15 17:14:33 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a pipe interface.
 * **************************************************************************/

#include <io/IORingBuffer.H>
#include <io/StreamServer.H>
#include <meta/MetaStreamServerPipe.H>
#include <stub/StubFileLinuxClientStream.H>
#include <cobj/XHandleTrans.H>
#include <io/FileLinux.H>
#include <misc/HashSimple.H>

#define MAX_PIPE_LINEBUF 1024*64

// FIXME: fix stub-compiler grammar so we don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class StreamServerPipe : public StreamServer
{
    struct PipeClientData;
    PipeClientData* pclnt(XHandle xhandle);
    IORingBuffer rb;

    // references internal to address space
    // sval refcount;	// number of internal references
    sval wcount;	// number of writers
    sval rcount;	// number of readers

    // If we have a named pipe, we want an upcall back to the filesystem
    // to get rid of the control reference to the object.  Currently,
    // we don't do this, so we instead have a flag, neverHadWriter,
    // to logically reset the pipe so it will behave like a new one
    // next time someone opens it from the name in the file system. 
    bool neverHadWriter;

    // Determine what ops are possible now
    // This calculates a global view of the availability,
    // not a client-specific one.  Each client may see different
    // availability depending on whether they are a writer or reader.
    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData *client=NULL);
    bool readersExist();  //Assumes lock is held
    bool writersExist();  //Assumes lock is held

public:
    virtual ~StreamServerPipe() {
	rb.destroy();
    }

protected:
    void init() {
	_lock.init();

	// refcount = 0;
	wcount = 0;
	rcount = 0;
	neverHadWriter = true;
	rb.init(MAX_PIPE_LINEBUF);
    }

    DEFINE_GLOBAL_NEW(StreamServerPipe);

    virtual SysStatus destroy();
    virtual SysStatus flush();

    SysStatusUval locked_readInternal(struct iovec* vec, uval len);
    SysStatusUval locked_writeInternal(struct iovec* vec, uval len);

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

public:
    DEFINE_ACCESS_SERVER(StreamServerPipe, MetaObj::controlAccess, 
			 MetaObj::none);

    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type = 0);

    static void ClassInit();
    static SysStatus Create(IORef &ref);

    virtual SysStatus detach();
    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle,
				  __CALLER_PID caller) {
	return giveAccessByServer(oh, toProcID, match, nomatch);
    }


__xprivate:

    virtual SysStatus _createReader(__out ObjectHandle &oh, __in ProcessID pid);

    virtual SysStatus _createWriter(__out ObjectHandle &oh, __in ProcessID pid);
    
    static SysStatus _Create(__out ObjectHandle &ohr, __out ObjectHandle &ohw,
			     __CALLER_PID caller);

    static SysStatus _Create(__out ObjectHandle &oh, __CALLER_PID caller );

};
#endif /* #ifndef __STREAM_SERVER_PIPE_H_ */
