#ifndef __LINUX_PTY_SERVER_H_
#define __LINUX_PTY_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxCharDev.H,v 1.4 2005/07/15 17:14:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a hardware character device interface.
 * **************************************************************************/

#include <io/StreamServer.H>
#include <meta/MetaStreamServer.H>
#include <stub/StubFileLinuxClientStream.H>
#include <cobj/XHandleTrans.H>
#include <io/IORingBuffer.H>
#include <io/FileLinux.H>
#include <misc/HashSimple.H>

extern "C"{
#define __KERNEL__
#undef major
#undef minor
// defines to exclude headers that cause conflicts
#define _LINUX_KERNEL_H
#define _LINUX_SCHED_H
#define _PPC64_PAGE_H

#ifdef WNOHANG
#undef WNOHANG
#endif
#ifdef WUNTRACED
#undef WUNTRACED
#endif

//#include <lk/fixup.H>
#include <linux/kdev_t.h>
//#include <linux/k42devfs.h>
#include <lk/Wait.H>
#undef __KERNEL__
}
#include <meta/MetaDevOpener.H>
#include <stub/StubDevOpener.H>
class LinuxCharDev;
typedef LinuxCharDev** LinuxCharDevRef;
class CharDevOpener;
typedef CharDevOpener** CharDevOpenerRef;

class LinuxCharDev : public StreamServer {
protected:
    friend class CharDevOpener;
    dev_t devNum;
    struct DevInfo *di;
    uval availMask;
    virtual void notify(unsigned long arg);

    LinuxCharDev(): availMask(0) {};

    struct ClientData :public StreamServer::ClientData{
    public:
	ProcessID pid;
	uval openFlags;
	DEFINE_GLOBAL_NEW(ClientData);
	ClientData(ProcessID proc):pid(proc) {};
	virtual ~ClientData() {};
    };
    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData *client=NULL);
    ClientData *getClientData(XHandle xh) {
	return (ClientData*)XHandleTrans::GetClientData(xh);
    }
    DEFINE_ACCESS_SERVER(StreamServer,
			 MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			 MetaObj::none);
    DEFINE_GLOBAL_NEW(LinuxCharDev);
    DEFINE_REFS(LinuxCharDev);
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
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type = 0);

    virtual SysStatus flush() { return 0; };

    /* exported functions */
    virtual SysStatusUval _ioctl(__in uval request, __inout uval &size,
				 __inoutbuf(size:size:size) char* arg,
				 __XHANDLE xhandle);
    static SysStatus ClassInit(VPNum vp);
    static SysStatus Create(ObjectHandle &oh, TypeID &type,
			    dev_t num, ProcessID pid,  uval oflag);

    virtual SysStatus destroy();
};


// A CharDevOpener is something that know how to open a particular
// char device.  IOW, it is the object that handles open() operations
// on the appropriate devfs files.
class CharDevOpener: public Obj {
protected:
    friend class LinuxCharDev;
    typedef HashSimpleLocked<dev_t, CharDevOpenerRef, AllocGlobal, 0>
    DeviceHash;
    static DeviceHash *charDevs;

public:
    dev_t devNum;
    DEFINE_GLOBAL_NEW(CharDevOpener);
    DEFINE_ACCESS_METASERVER(MetaDevOpener, MetaObj::none, MetaObj::none);
    DEFINE_REFS(CharDevOpener);
    static SysStatus Create(unsigned int major, unsigned int minor,
			    ProcessID devfsPID, CharDevOpenerRef &ref,
			    ObjectHandle &opener);
    virtual SysStatus _open(__out ObjectHandle &oh,
			    __out TypeID &type,
			    __in  ProcessID pid,
			    __in  uval oflag,
			    __in  uval token);
    virtual SysStatus exportedXObjectListEmpty() {
	return destroy();
    }
    virtual SysStatus destroy();
    friend SysStatus CharCreateDevice(ObjectHandle dirOH,
				      const char *name, mode_t mode,
				      unsigned int major,
				      unsigned int minor, ObjectHandle &device,
				      void* &devData);
};



#endif /* #ifndef __LINUX_PTY_SERVER_H_ */
