#ifndef __STREAM_SERVER_SOCKET_H
#define __STREAM_SERVER_SOCKET_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerSocket.H,v 1.18 2005/07/15 17:14:32 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of Unix domain sockets
 * **************************************************************************/

#include <io/SocketServer.H>
#include <meta/MetaStreamServerSocket.H>
#include <misc/HashSimple.H>
#include <cobj/CObjRootSingleRep.H>
// make the stub compiler happy about GenState and FileLinux
#ifndef EXPORT_STREAMSERVERSOCKET
#define EXPORT_STREAMSERVERSOCKET
#include <io/IORingBuffer.H>
#include <io/GenState.H>
#include <sys/uio.h>
#include <io/FileLinux.H>
#endif /* #ifndef EXPORT_STREAMSERVERSOCKET */

#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
// fixme  set permission on this class
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class StreamServerSocket;
typedef StreamServerSocket** StreamServerSocketRef;
/**
 * Implementation of Unix domain sockets
 */
class StreamServerSocket : public SocketServer
{
private:
    uval socketDomain;		// this better be unix for us
    uval socketType;		// stream or datagram

#ifndef _IN_STUBGEN
    struct SockClientData;
    static SockClientData* sclnt(XHandle xhandle);

    /**
     * Determine what ops are possible now
     * This calculates a global view of the availability,
     * not a client-specific one.  Each client may see different
     * availability depending on whether they are a writer or reader.
     */
    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData *client=NULL);

    IORingBuffer rb1;
    IORingBuffer rb2;
    bool readersExist();  //Assumes lock is held
    bool writersExist();  //Assumes lock is held

    uval readByteCount1;
    uval readByteCount2;
    uval writeByteCount1;
    uval writeByteCount2;

    struct AncillaryData {
	AncillaryData() {}
	uval index;
	char *fdData;
	uval fdDataSize;
    };

    ListSimple<AncillaryData *, AllocGlobal> fdTransferList1;
    ListSimple<AncillaryData *, AllocGlobal> fdTransferList2;

    // each represents a socket that has done a successful connect
    struct ConnectSocketData {
        ConnectSocketData(ObjectHandle oh) : socketOH(oh) { /* empty body */ }
	ObjectHandle socketOH;
    };

    /**
     * list of OH of socket server object that have connected.  To be
     * determined if this data structure is appropriate.  Need a count
     * of items in the list.  Fixme find an implementation or add one
     * or a count to this
     */
    ListSimple<ConnectSocketData*, AllocGlobal> connectedSocketList;

    uval socketIsBound;		// flag
    uval socketIsListening;	// flag
    uval socketIsConnected;	// flag
    uval listenBacklog;		// count
    uval useCount;		
    
    uval traceThisSocket;
#endif	// #ifndef _IN_STUBGEN

public:
    /* definition of all the getAccess methods */
    DEFINE_ACCESS_SERVER(StreamServerSocket, MetaObj::controlAccess,
			 MetaObj::none);

    StreamServerSocket(uval domain, uval type);
    virtual ~StreamServerSocket();

    static void ClassInit();

    static SysStatus Create(StreamServerSocketRef &Ref, uval domain = AF_UNIX,
			    uval type = SOCK_STREAM, uval traceFlag = 0);
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type = 0);
    virtual SysStatus handleXObjFree(XHandle xhandle);
    virtual SysStatus getType(TypeID &id);
    virtual SysStatus setEndSocketClientData(XHandle xhandle);
    virtual SysStatus setSocketIsConnected(uval connected);
    virtual SysStatus getSocketIsConnected(uval &connected);

protected:
    DEFINE_GLOBAL_NEW(StreamServerSocket);

    SysStatusUval locked_readInternal(IORingBuffer &rb, struct iovec* vec, 
				      uval len, uval *readByteCount);
    SysStatusUval locked_writeInternal(IORingBuffer &rb, struct iovec* vec, 
				       uval len, uval *writeByteCount);

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

    
    virtual SysStatus locked_addFDToMessage(__XHANDLE xhandle, char *data, 
					    uval dataLen);
    virtual SysStatus locked_getFDFromMessage(__XHANDLE xhandle,  
					      char *sourceData,
					      char *data,
					      uval &buflen);

__xpublic:
    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen);


    virtual SysStatus _lazyReOpen(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle);

    virtual SysStatus _dup(__out ObjectHandle &oh, __in ProcessID toProcID,
			   __XHANDLE xhandle);

__xprotected:
    // Override _giveAccess to allow for client data to be copied

    // Duplicate the XObj.
    // This is our mechanism for passing capabilities, it returns
    // an object handle for (possibly) new XObj.
    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __XHANDLE xhandle) __xa(controlAccess);

    // Duplicate the XObj reducing access
    // This is our mechanism for passing capabilities, it returns
    // an object handle for new XObj.
    // It is an error to ask to pass a right that you don't have
    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) __xa(controlAccess);
__xprivate:
    static SysStatus _CreateSocketPair(__out ObjectHandle &socket1,
				       __out ObjectHandle &socket2,
				       __CALLER_PID caller);
    static SysStatus _CreateSocket(__out ObjectHandle &socket,
				   __out uval &clientType,
                                   __in sval domain, __in sval type,
                                   __in sval protocol, __in uval traceFlag,
				   __CALLER_PID caller);

    virtual SysStatus _bind(__inoutbuf(addrLen:addrLen:addrLen) char *addr,
                            __inout uval &addrLen, __XHANDLE xhandle) {
	// this form of bind is not supported
	return _SERROR(2892, 0, ENOTSUP);
    }

    virtual SysStatus _queueToConnect(__in ObjectHandle oh,
				      __XHANDLE xhandle);
    /**
     * override to return error, this form not supported for unix 
     * domain sockets
     */
    virtual SysStatus _connect(__inbuf(addrLen) const char *addr,
			       __in uval addrLen,
			       __out GenState& moreAvail,
			       __XHANDLE xhandle);

    virtual SysStatus _bind();
    virtual SysStatus _getSocketObj(__out ObjectHandle &oh, __in ProcessID pid);

    virtual SysStatus _setsockopt(__in uval level, __in uval optname,
				  __inbuf(optlen) char *optval,
				  __in uval optlen, __XHANDLE xhandle);

    virtual SysStatus _getsockopt(__in uval level, __in uval optname,
				  __outbuf(optlen:optlen) char *optval,
				  __inout uval *optlen, __XHANDLE xhandle);

    virtual SysStatus _listen(__in sval backlog);
    virtual SysStatus _accept(__out ObjectHandle &oh,
                              __inoutbuf(len:len:len) char* addr,
                              __inout uval& len,
                              __out GenState &moreAvail,
                              __XHANDLE xhandle);
    virtual SysStatus _socketTrace(__in uval onOrOff);
};

typedef StreamServerSocket** StreamServerSocketRef;
#endif // ifndef __STREAM_SERVER_SOCKET_H
