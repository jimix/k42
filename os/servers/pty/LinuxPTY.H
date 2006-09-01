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
 * $Id: LinuxPTY.H,v 1.11 2005/07/15 17:14:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a pty interface.
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
#include <linux/k42devfs.h>
#include <lk/Wait.H>
#undef __KERNEL__
}
#include <meta/MetaDevOpener.H>
#include <stub/StubDevOpener.H>
class LinuxPTYServer;
typedef LinuxPTYServer** LinuxPTYServerRef;
class CharDevOpener;
typedef CharDevOpener** CharDevOpenerRef;

class LinuxPTYServer : public StreamServer {
protected:
    friend class CharDevOpener;
    dev_t devNum;
    struct OpenLineInfo *oli;
    uval availMask;
    struct TTYEvents;
    friend struct TTYEvents;
    virtual void notify(unsigned long arg);
    struct TTYEvents: public __wait_queue{
	volatile uval callbackArg;
	LinuxPTYServerRef obj;
	uval bits;
	TTYEvents(uval _bits):callbackArg(~0ULL), bits(_bits) {
	    func = &TTYwake;
	    task_list.next = &task_list;
	    task_list.prev = &task_list;
	};
	virtual void realWakeup() {
	    uval arg;
	    do {
		arg = Swap(&callbackArg, ~1ULL);
		DREF(obj)->notify(bits);
	    } while (!CompareAndStoreSynced(&callbackArg, ~1ULL, ~0ULL));
	}
	static void NotifyThread(uval arg) {
	    TTYEvents* t = (TTYEvents*)arg;
	    t->realWakeup();
	}
	static int TTYwake(wait_queue_t *wait, unsigned mode, int sync) {
	    TTYEvents* t = (TTYEvents*)wait;
	    t->wakeup((uval)wait->task);
	    return 1;
	}
	virtual void wakeup(unsigned long arg) {
	    uval x = Swap(&callbackArg,arg);
	    if (x == ~0ULL) {
		Scheduler::ScheduleFunction(TTYEvents::NotifyThread,
					    (uval)this);
	    }
	}
        DEFINE_NOOP_NEW(TTYEvents);
        virtual ~TTYEvents() { }
    };

    wait_queue_head_t *readQ;
    wait_queue_head_t *writeQ;
    TTYEvents readCB;
    TTYEvents writeCB;

    LinuxPTYServer(): availMask(0),
		      readQ(NULL), writeQ(NULL),
		      readCB(FileLinux::READ_AVAIL),
		      writeCB(FileLinux::WRITE_AVAIL) {};

    struct ClientData :public StreamServer::ClientData{
    public:
	ProcessID pid;
	uval openFlags;
	uval ctty;	//value to set current->tty with
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
    DEFINE_GLOBAL_NEW(LinuxPTYServer);
    DEFINE_REFS(LinuxPTYServer);
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

__xpublic:
    virtual SysStatus _getStatus(__out struct stat &status);

};


// A CharDevOpener is something that know how to open a particular
// char device.  IOW, it is the object that handles open() operations
// on the appropriate devfs files.
class CharDevOpener: public Obj {
protected:
    friend class LinuxPTYServer;
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
    friend SysStatus CreateDevice(ObjectHandle dirOH,
				  const char *name, mode_t mode,
				  unsigned int major,
				  unsigned int minor, ObjectHandle &device,
				  void* &devData);
};



#endif /* #ifndef __LINUX_PTY_SERVER_H_ */
