#ifndef __STREAM_SERVER_H_
#define __STREAM_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServer.H,v 1.36 2005/07/15 17:14:25 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Stream interface exported by server
 ***************************************************************************/
#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <io/FileLinux.H>
#include <io/MemTrans.H>
#ifndef EXPORT_STREAMSERVER
#define EXPORT_STREAMSERVER
#include <io/GenState.H>
#include <sys/types.h>
#include <sys/ProcessLinux.H>
#include <sys/uio.h>
#include <sys/stat.h>
#endif /* #ifndef EXPORT_STREAMSERVER */


#ifndef _IN_STUBGEN
#include <stub/StubFileLinuxClientStream.H>
class StreamServer;
typedef StreamServer** StreamServerRef;
#endif /* #ifndef _IN_STUBGEN */
struct iovec;

class StreamServer : public Obj __xbase(Obj) {
protected:
    typedef BLock LockType;
    LockType _lock;
#ifdef _IN_STUBGEN
    //stub compilation dies if we use "class" instead of "struct"
    struct ClientData;
    static ClientData* clnt(XHandle xhandle);
#else /* #ifdef _IN_STUBGEN */
    // Object for receiving notifications of ring events
    struct IOMemEvents : public MemTrans::MTEvents{
	DEFINE_GLOBAL_NEW(IOMemEvents);
	virtual void nowDead(MemTransRef mtr);
    };
    static const uval NOPRESSURE=~0ULL;

    struct ClientData {
    public:
	StubFileLinuxClientStream stub;
	volatile GenState available;    // state client thinks stream is in
	uval silence; //if this reaches 0, don't async notify
	MemTransRef mtr;
	uval blocked;     // SMT address that must be handed back
	XHandle smtXH;
	IOMemEvents *memEventHandler;

	struct IOCompInfo {
	    sval32 bytesLeft;
	    uval32 ioLen;
	    uval32 bufLen;
	    char* buf;
	};
	IOCompInfo ici;


	DEFINE_GLOBAL_NEW(ClientData);
	ClientData();
	void setOH(ObjectHandle oh) {stub.setOH(oh);};
	uval isValidOH() { return stub.getOH().valid(); };
	virtual void setAvail(GenState &ma);  // sync notification
	virtual SysStatus signalDataAvailable(GenState avail); // async
	virtual ~ClientData() {};
	virtual struct IOCompInfo* getICI() { return &ici; };
    };

    // ClientData for xhandles that don't want any notifications
    struct NullClientData : public ClientData{
    public:
	DEFINE_GLOBAL_NEW(NullClientData);
	virtual void setAvail(GenState &ma) {};
	virtual SysStatus signalDataAvailable(GenState avail) { return 0;};
	virtual ~NullClientData() {};
	virtual struct IOCompInfo* getICI() { return NULL; };
    };

    static ClientData* clnt(XHandle xhandle);

#endif /* #ifdef _IN_STUBGEN */
    virtual void calcAvailable(GenState& avail, ClientData *client=NULL)=0;
    virtual void lock() { _lock.acquire(); };
    virtual void unlock() { _lock.release(); };

    uval seqnum;  //Availability sequence number
public:
    enum { DIRECT,
	   OFFSET
    };

    // class code errors
    enum {
	WOULD_BLOCK=56
    };

    static void BeingFreed(XHandle xhandle);
    virtual SysStatus exportedXObjectListEmpty();

    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus signalDataAvailable();
    virtual SysStatus locked_signalDataAvailable();

    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type = 0);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch) {
      return giveAccessByServer(oh,toProcID, match, nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
      return giveAccessByServer(oh,toProcID);
    }

    virtual SysStatus flush();
    virtual SysStatus destroy();
    virtual SysStatusUval recvfrom(struct iovec *vec, uval veclen, uval flags,
				   char *addr, uval &addrLen,
				   GenState &moreAvail, 
				   void *controlData, uval &controlDataLen,
				   __XHANDLE xhandle) = 0;
    virtual SysStatusUval sendto(struct iovec* vec, uval veclen, uval flags,
				 const char *addr, uval addrLen,
				 GenState &moreAvail, 
				 void *controlData, uval controlDataLen,
				 __XHANDLE xhandle) = 0;

__xpublic:
    virtual SysStatusUval _recvfrom(
	__outbuf(__rc:len) char *buf, __in uval len, __in uval flags,
	__outbuf(addrLen:addrLen) char *addr, __inout uval &addrLen,
	__inout uval &bytesLeft, __inout GenState &moreAvail,
	__outbuf(controlDataLen:controlDataLen) char *controlData,
	__inout uval &controlDataLen, __XHANDLE xhandle) __xa(read);


    virtual SysStatusUval _sendto(
	__inbuf(len) char *buf, __in uval len, __in uval flags,
	__inbuf(addrLen) const char *addr, __in uval addrLen,
	__in uval bytesLeft, __inout GenState &moreAvail,
	__inbuf(controlDataLen) const char *controlData, 
	__in uval controlDataLen,
	__XHANDLE xhandle) __xa(write);

    virtual SysStatusUval _sendtoVirt(__inbuf(len) struct iovec* vec,
				      __in uval len,
				      __in uval flags,
				      __inbuf(addrLen) const char *addr,
				      __in uval addrLen,
				      __inout GenState &moreAvail,
				      __XHANDLE xhandle) __xa(write) {
	return _SERROR(2087, 0, EOPNOTSUPP);
    }

    virtual SysStatusUval _recvfromVirt(__inbuf(len) struct iovec* vec,
					__in uval len,
					__in uval flags,
					__outbuf(addrLen:addrLen) char *addr,
					__inout uval &addrLen,
					__inout GenState &moreAvail,
					__XHANDLE xhandle) {
	return _SERROR(2088, 0, EOPNOTSUPP);
    }
    virtual SysStatus _registerCallback(__in ObjectHandle callback,
					__XHANDLE xhandle);

    virtual SysStatus _flush();

    virtual SysStatusUval _getAvailability(__out GenState &avail,
					   __XHANDLE xhandle);

    virtual SysStatusUval _ioctl(__in uval request, __inout uval &size,
				 __inoutbuf(size:size:size) char* arg,
				 __XHANDLE xhandle);

    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen);
    // call from kernel to re-open this file
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	return _giveAccess(oh, toProcID, match, nomatch, xhandle);
    }

    virtual SysStatus _dup(__out ObjectHandle &oh, __in ProcessID toProcID,
			   __XHANDLE xhandle) {
	return _giveAccess(oh, toProcID, xhandle);
    }

    virtual SysStatus _setsockopt(__in uval level, __in uval optname,
				  __inbuf(optlen) char *optval,
				  __in uval optlen, __XHANDLE xhandle) {
	return _SERROR(2905, 0, ENOTSOCK);
    }

    virtual SysStatus _getsockopt(__in uval level, __in uval optname,
				  __outbuf(optlen:optlen) char *optval,
				  __inout uval *optlen, __XHANDLE xhandle) {
	return _SERROR(2911, 0, ENOTSOCK);
    }

    virtual SysStatus _getStatus(__out struct stat &status);


};
#endif /* #ifndef __STREAMSERVER_H_ */
