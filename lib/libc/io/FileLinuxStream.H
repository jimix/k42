#ifndef __FILE_LINUX_STREAM_H_
#define __FILE_LINUX_STREAM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxStream.H,v 1.67 2005/07/15 17:14:22 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality
 *
 * WARNING, any calls to operations that block on reading will deactivate
 *   and activate, operations that block on reading must protected themselves
 * **************************************************************************/

#include <io/FileLinux.H>
#include <io/IOBufferList.H>
#include <stub/StubStreamServer.H>
#include <meta/MetaFileLinuxClientStream.H>
#include <io/FileLinuxClientStream.H>
#include <io/MemTrans.H>

// object dynamically allocated to hold a stub object
struct TransStreamServer {
protected:
    StubStreamServer stub;
public:
    enum { TRANS_NONE, TRANS_PPC, TRANS_VIRT, TRANS_SMT };
    const uval32 transType;
    /*
     * C++ magic, makes sure all destructors are virtual
     * so that delete called with size and can be overloaded
     */
    DEFINE_NOOP_NEW(TransStreamServer);
    virtual ~TransStreamServer() {};
    TransStreamServer(ObjectHandle oh, uval type):
	stub(StubObj::UNINITIALIZED), transType(type) {
	stub.setOH(oh);
    };
    virtual uval maxBytesRW(uval amt) {
	return MIN(amt, FileLinux::SMTCutOff);
    }
    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  GenState &moreAvail, void *controlData,
				  uval &controlDataLen)= 0;
    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				  uval startOffset, GenState &moreAvail,
				  void *controlData,
				  uval controlDataLen) = 0;
    void setOH(ObjectHandle oh) {
	stub.setOH(oh);
    }
#define getStub(type, holder) ((type)holder->__getStub())
    StubStreamServer* __getStub() {
	return &stub;
    }
    ProcessID getPid() {
	return stub.getPid();
    }

    ObjectHandle getOH() {
	return stub.getOH();
    }
};


struct TransPPCStream : public TransStreamServer {
    TransPPCStream(ObjectHandle oh) :TransStreamServer(oh, TRANS_PPC) {};
    DEFINE_GLOBAL_NEW(TransPPCStream);
    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  GenState &moreAvail, void *controlData,
				  uval &controlDataLen);
    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				  uval startOffset, GenState &moreAvail,
				  void *controlData,
				  uval controlDataLen);
    virtual uval maxBytesRW(uval amt) {
	return amt;
    }
    virtual ~TransPPCStream() {};
};

struct TransVirtStream : public TransStreamServer {
    TransVirtStream(ObjectHandle oh) :TransStreamServer(oh, TRANS_VIRT) {};
    DEFINE_GLOBAL_NEW(TransVirtStream);
    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  GenState &moreAvail, void *controlData,
				  uval &controlDataLen);
    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				  uval startOffset, GenState &moreAvail,
				  void *controlData, uval controlDataLen);
    virtual uval maxBytesRW(uval amt) {
	return amt;
    }
    virtual ~TransVirtStream() {};
};



class FileLinuxStream : public FileLinuxClientStream {
protected:
    DEFINE_ACCESS_SERVER(FileLinuxClientStream,
			 MetaObj::controlAccess,MetaObj::none);
    DEFINE_GLOBAL_NEW(FileLinuxStream);

    class MTEvents: public MemTrans::MTEvents {
    public:
	DEFINE_GLOBAL_NEW(MTEvents);
	MTEvents():mtr(NULL) { /* empty body */ }
	MemTransRef mtr;
	XHandle smtXH;
    };
    MTEvents *mth;

    typedef FairBLock LockType;
    LockType objLock;

    // If NULL, then no thread is trying to update state in the
    // waiters list. If it is not NULL, then any attempt to grab
    // waiters lock for the purpose of doing the update procedure
    // should abort, and leave it to the current lock holder to do it.
    // This avoids deadlock on performing a setAvailable after an I/O
    // operation that was initiated from a callback to an IONotif from
    // setAvailable.
    volatile Thread* statusUpdater;

    // the head element is the one available for subsequent reads,
    // all other elements are kept around because they have active
    // references
    IOBufListReg rBufList;		// buffers currently reading
    IOBufListReg wBufList;		// buffers currently writing

    // implementation specific to this class
    // facility that implements the comminucation and coordination
    // between the client object (a specialization of this object)
    // and the server object.  There are implementation of 
    // TransStreamServer which can be used only by the kernel
    // (e. g., TransVirtStream) and implementation that can be used
    // by the kernel or a server process (e. g., TranPPCStream).
    TransStreamServer *stubHolder;

    volatile GenState available;

    void initSMT();

    void init(ObjectHandle oh, FileLinuxRef useThis);

    // Called to notify the object that the new state of remote
    // implementation is now "avail".  This method will make the
    // appropriate callbacks to anyone registered via "notify".  Note
    // that avail is in-out parameter as it may change due to callback
    // actions.
    void setAvailable(GenState avail);

    void locked_completeDestruction();

    //checks if object satisfies "condition"
    //returns 1 if condition satisfied, 0 if not
    SysStatus checkAvail(ThreadWait **tw, uval condition);

    //For debugging, returns the *real*, uncached state
    virtual SysStatus debugAvail(GenState &raw, GenState &avail);

    SysStatus registerCallback();

    // internal operation to read/write data
    SysStatusUval locked_recvmsg(struct msghdr &msg, uval flags,
				 ThreadWait **tw, GenState &moreAvail,
				 void *controlData, uval &controlDataLen);

    SysStatusUval locked_sendmsg(struct msghdr &msg, uval flags,
				 ThreadWait **tw, GenState &moreAvail,
				 void *controlData, uval controlDataLen);
    virtual SysStatus _notifyKeyed(IONotif *ion, KeyedNotif* &existing);

public:
    class FDTransferData {
    public:
	FDTransferData(); // Create an empty one

	// Initialised with data from previous getDataBlock call
	FDTransferData(char *dataBlock);
	~FDTransferData();


	uval getNumEntries() const;

	struct EntryLayout {
	    uval entrySize;
	    TypeID dataForType;
	    ObjectHandle oh;
	    char rawData[0];
	};
	EntryLayout *getEntry(uval entryNum);

	void addEntry(char *entryDataBlock, uval entryDataBlockSize, 
		      ObjectHandle &oh, TypeID dataForType);
	// copy of data taken

	SysStatus getDataBlock(char **data, uval *blockSize);
	uval getDataBlockSize() const;

    private:
	uval *numEntries;
	uval entryOffsetIndexSize;
	uval *entryOffsetIndex;
	char *dataBlock;
	uval dataBlockSize;
	uval dataBlockUsed;

    };

    virtual SysStatus getType(TypeID &type) {
	type = FileLinux_STREAM;
	return 0;
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch) {
	return getStub(StubStreamServer*,stubHolder)->_giveAccess(oh, toProcID,
							      match, nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
	return getStub(StubStreamServer*,stubHolder)->_giveAccess(oh, toProcID);
    }

    virtual SysStatusUval readv(struct iovec* vec, uval vecCount,
				ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval writev(const struct iovec* vec, uval vecCount,
				 ThreadWait **tw,GenState &moreAvail);

    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				 ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  ThreadWait **tw, GenState &moreAvail);

    // We depend on the destructor to do cleanup notification list
    // after increment of generation count (hence in dtor, not
    // destroy()).  This operation must be done once we know that no
    // other notifications can succeed (which will be the case since
    // the ref is no longer valid --- it's a deleted object).
    virtual ~FileLinuxStream();
    virtual SysStatus destroy();

    virtual SysStatus flush();

    virtual SysStatus getStatus(FileLinux::Stat *status);

    // instantiate a stream based on passed in oh
    static SysStatus Create(FileLinuxRef &r, ObjectHandle oh,
			     uval oflags, FileLinuxRef useR = NULL);

    static SysStatus Pipe(FileLinuxRef& newPipeR, FileLinuxRef& newPipeW);

    static SysStatus SocketPair(FileLinuxRef &newSocket1,
				FileLinuxRef &newSocket2,
				uval domain, uval type, uval protocol);

    virtual SysStatus dup(FileLinuxRef& newStream);

    virtual SysStatus ioctl(uval request, va_list args);

    virtual SysStatusUval FIXMEpostFork(ObjectHandle oh);

    virtual SysStatus lazyGiveAccess(sval fd);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);
    virtual void getStateTransferData(char *transferData);
    virtual uval getStateTransferDataSize() const;

    // Register IONotif object for notifications of state changes
    virtual SysStatus notify(IONotif *comp);

    // Described in FileLinux.H
    virtual SysStatus notifyKeyed(KeyedNotif *kn, KeyedNotif* &existing);

    virtual SysStatus getState(GenState &state) {
	state = available;
	return 0;
    }

    virtual SysStatus setsockopt(uval level, uval optname,
				 const void *optval, uval optlen);
    virtual SysStatus getsockopt(uval level, uval optname,
				 const void *optval, uval *optlen);

    /* --------------- ASI interface ------------------- */
    /*
     * allocate a buffer of date for reading.  The buffer
     * will actually contain the values "read"
     * returns the actual length if at end of file
     */
    virtual SysStatusUval readAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatusUval readAllocAt(uval len, uval off, At at, char * &bf,
				      ThreadWait **tw);

    // indicate that the buffer is no longer needed
    virtual SysStatus readFree(char *ptr);

    // allocate an output buffer to be filled
    virtual SysStatusUval writeAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatusUval writeAllocAt(uval len, uval off, At at,
				       char * &bf, ThreadWait **tw);
    // indicate that the buffer has been written
    virtual SysStatus writeFree(char *ptr);

    /*
     * Operations same as above alloc ones, but lock is acquired and
     * freed explicitly before calling them. They typically assert lock
     * acquisition.
     */
    virtual SysStatus lock();
    virtual SysStatus unLock();

    virtual SysStatusUval locked_readAlloc(uval len, char * &buf,
					   ThreadWait **tw);
    virtual SysStatusUval locked_readAllocAt(uval len, uval off, At at,
					     char *&bf, ThreadWait **tw);
    virtual SysStatusUval locked_readRealloc(char *prev, uval oldlen,
					     uval newlen, char * &buf,
					     ThreadWait **tw);
    virtual SysStatus locked_readFree(char *ptr);
    virtual SysStatusUval locked_writeAllocAt(uval len, uval off, At at,
					      char *&bf, ThreadWait **tw);
    virtual SysStatusUval locked_writeAlloc(uval len, char * &buf,
					    ThreadWait **tw);
    virtual SysStatusUval locked_writeRealloc(char *prev, uval oldlen,
					      uval newlen, char * &buf,
					      ThreadWait **tw);
    virtual SysStatus locked_writeFree(char *ptr);
    virtual SysStatus locked_flush(uval release = 0);

    /* --------------- end ASI interface ------------------- */
    // upcall from server side:
__xprivate:
    // change in status, typically data has become available
    // the state passed is ored into availability in client, as
    // described in:
    //      "lib/libc/io/README.html#Blocking in application"
    virtual __async SysStatus _signalDataAvailable(GenState avail);
};

#endif /* #ifndef __FILE_LINUX_STREAM_H_ */
