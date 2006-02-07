#ifndef __FILE_LINUX_H_
#define __FILE_LINUX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinux.H,v 1.129 2005/07/15 17:14:20 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines standard interface for a Linux file.
 * **************************************************************************/

#include <defines/experimental.H>

#include <sys/ppccore.H>

#include <io/PathName.H>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/vfs.h>

#include <fslib/direntk42.H>

#include <io/iovec.H>

#include <misc/AutoList.H>

#include <sync/BLock.H>
#include <io/GenState.H>
#include <sys/sysmacros.h>

// Linux kernel header, but safe --- just has #defines for device major numbers
#include <linux/major.h>

#ifndef _IN_STUBGEN
#include <scheduler/Scheduler.H>
#endif /* #ifndef _IN_STUBGEN */

// prototype of function to be called when data becomes available
// "avail" specifies the new bitmap, after the most recent changes
// have been applied
typedef void (*AvailFunction)(uval, uval avail);
class FileLinux;

class IONotif: public AutoListNode {
public:
    enum NotifFlags {
	Persist     = 0x0001, // Notification object is not to be detached
	DelMe       = 0x0002, // Delete this on detach
	Keyed	    = 0x0004, // There is a key for this object
			      // (object is of type KeyedNotif)
    };
    uval flags;
    uval condition;
    volatile GenState available;
#ifndef _IN_STUBGEN
    IONotif (uval waitFor)
	:AutoListNode(), flags(0), condition(waitFor) { /* empty body */};

#else /* #ifndef _IN_STUBGEN */
    IONotif (uval waitFor);
#endif /* #ifndef _IN_STUBGEN */
    virtual ~IONotif () {};
    virtual void ready(FileLinuxRef fl, uval state) = 0;
    DEFINE_GLOBALPADDED_NEW(IONotif);
};

//This is what threads look to block for IO
class ThreadWait {
protected:
    ThreadID id;
public:
#ifndef _IN_STUBGEN
    ThreadWait():id(Scheduler::GetCurThread()) {/* empty body */};
    uval unBlocked() {
	return id == Scheduler::NullThreadID;
    }
#else /* #ifndef _IN_STUBGEN */
    ThreadWait();
    uval unBlocked();
#endif /* #ifndef _IN_STUBGEN */
    // MUST CALL DESTROY BEFORE DESTRUCTOR
    virtual void destroy() = 0;
    virtual ~ThreadWait() {};
    DEFINE_GLOBALPADDED_NEW(ThreadWait);
};

class ThreadNotif: public IONotif, public ThreadWait{
public:
    ThreadNotif (uval waitFor) : IONotif (waitFor), ThreadWait() {/*empty*/};
#ifndef _IN_STUBGEN
    virtual void destroy() {
	detach();
    }
    virtual void ready(FileLinuxRef fl, uval state) {
	ThreadID x = id;
	id = Scheduler::NullThreadID;
	Scheduler::Unblock(x);
    }
#else /* #ifndef _IN_STUBGEN */
    virtual void destroy();
    virtual void ready(FileLinuxRef fl, uval state);
#endif /* #ifndef _IN_STUBGEN */
    DEFINE_GLOBALPADDED_NEW(ThreadNotif);
    virtual ~ThreadNotif () {};
};


class KeyedNotif: public IONotif{
public:
    uval key;
#ifndef _IN_STUBGEN
    KeyedNotif (uval waitFor, uval k) : IONotif (waitFor), key(k) {
	flags |= IONotif::Keyed;
    };
#else /* #ifndef _IN_STUBGEN */
    KeyedNotif (uval waitFor, uval k);
#endif /* #ifndef _IN_STUBGEN */
    DEFINE_GLOBALPADDED_NEW(KeyedNotif);
    virtual ~KeyedNotif () {};
};

class FileLinux : public Obj {
private:
    static uval UMaskValue;
    static SysStatus GetTreeForTwoPaths(const char *oldpath,
					const char* newpath,
					uval &maxphlen,
					PathNameDynamic<AllocGlobal> *&oldp,
					uval &oldlen,
					PathNameDynamic<AllocGlobal> *&newp,
					uval &newlen, ObjectHandle &toh);

public:
    // data for a LazyReOpen
    struct LazyReOpenData {
	uval32 openFlags;	// open flags that were used when created
	uval32 transType;       // type of transport object used
    };

protected:
    class CurrentWorkingDirectory;
    static CurrentWorkingDirectory *cwd;

    volatile uval32 openFlags;
    volatile uval32 refCount;
    FileLinux() : openFlags(0), refCount(1) { /* empty body */ }

    LockedAutoListHead waiters;

    struct IoctlDesc {
	uval number;
	uval arg_size; // if bit 1<<64 is set, arg is not a pointer
    };
    static IoctlDesc ioctl_table[];

    SysStatus init(FileLinuxRef &useThis);

public:
    // Defines size of buffer for optimization for small non-shared files
    static const uval MAX_SMALL_FILE_PPCS = 4;

    // FIXME dilma: I believe we only 3 uvals instead of 4
    static const uval SMALL_FILE_PPC_MAX_LOAD =
		    PPCPAGE_LENGTH_MAX - 4*sizeof(uval) - sizeof(__XHANDLE);

    static const uval MAX_SMALLFILE_SIZE = 4 * PAGE_SIZE;

    typedef enum {CALLBACK_REQUEST_INFO, CALLBACK_REQUEST_SWITCH,
		  CALLBACK_INVALID} RequestType;
    typedef enum {LAZY_INIT, NON_SHARED, SHARED, FIXED_SHARED,
		  TRANSITION,
		  INVALID_UT /* used for UseType on client data that has just
			      * been created or it has just been detached */
    } UseType;

    typedef enum {USETYPE_CALL_BACK, LOCK_CALL_BACK} CallBackType;

    /*
     * class specific error codes uses in the I/O space
     * the GenCode part of the error is ECANCELED
     */
    typedef enum { HIT_SYMBOLIC_LINK = 69, NODE_ALREADY_EXISTS = 100 };

    /*
     * This is an experiment, do we want to encapsulate standard Unix
     * structs in classes that make them easier to manipulate???
     */
    struct Stat : stat {
    public:
	// block size assumed for Linux
	static const uval STAT_BLOCK_SIZE = 512;
	uval isDir()	{return S_ISDIR(st_mode);}
	uval isFile()	{return S_ISREG(st_mode);}
	uval isChar()	{return S_ISCHR(st_mode);}
	uval isBlock()	{return S_ISBLK(st_mode);}
	uval isFIFO()	{return S_ISFIFO(st_mode);}
	uval isSocket()	{return S_ISSOCK(st_mode);}
	uval isSymLink() {return S_ISLNK(st_mode);}

	// typecheats a struct stat pointer to a Stat class
	static FileLinux::Stat *FromStruc(struct stat *s) {
	    return (Stat *)s;
	};

	// compares stat values
	SysStatus Compare (Stat *st);
	// auxilliary function
	static uval check(char expr, char *msg);
    };

// makedev is no longer a macro, so we need our own to initialize enums
#define k42makedev(major, minor)					\
	(uval(((uval(minor) & 0xffULL)					\
	 | ((uval(major) & 0xfffULL) << 8ULL)				\
	 | ((uval(minor) & ~0xffULL) << 12ULL)				\
	 | ((uval(major) & ~0xfffULL) << 32ULL))))

#define FileLinux_DIR		k42makedev(0,0) /*  directory */
#define FileLinux_FILE		k42makedev(0,1) /*  random access block, mappable */
#define FileLinux_VIRT_FILE	k42makedev(0,2) /*  user provides read/write methods */
#define FileLinux_SOCKET	k42makedev(0,3)	/*  socket */
#define FileLinux_PIPE		k42makedev(0,4)	/*  pipe */
#define FileLinux_STREAM	k42makedev(0,5)	/*  generic stream */
#define FileLinux_TTY		k42makedev(0,6) /*  isatty things */
#define FileLinux_FILEFIXEDSHARED		k42makedev(0,7) /*  like FileLinux_FILE */
	// unique rdevs for stat
#define FileLinux_CHR_NULL	k42makedev(1, 3)
#define FileLinux_CHR_ZERO	k42makedev(1, 5)
#define FileLinux_CHR_RANDOM	k42makedev(1, 8)
#define FileLinux_CHR_TTY	k42makedev(TTYAUX_MAJOR, 0)
#define FileLinux_BLK_SDA       k42makedev(8, 0)
#define FileLinux_BLK_SDB       k42makedev(8, 16)
#define FileLinux_BLK_SDC       k42makedev(8, 32)
#define FileLinux_UNKNOWN	~0ULL	/* Make sure this is always last */

  // The following values may be defined by the surrounding glib header files.
  // To avoid conflicts, we undefine the symbols before defining our own enum for them.
#ifdef HUP
#undef HUP
#endif /* #ifdef HUP */
#ifdef INVALID
#undef INVALID
#endif /* #ifdef INVALID */
#ifdef POLLIN
#undef POLLIN
#endif /* #ifdef POLLIN */
#ifdef POLLOUT
#undef POLLOUT
#endif /* #ifdef POLLOUT */
#ifdef POLLPRI
#undef POLLPRI
#endif /* #ifdef POLLPRI */
#ifdef POLLERR
#undef POLLERR
#endif /* #ifdef POLLERR */
#ifdef POLLNVAL
#undef POLLNVAL
#endif /* #ifdef POLLNVAL */
#ifdef POLLHUP
#undef POLLHUP
#endif /* #ifdef POLLHUP */
#ifdef POLLRDNORM
#undef POLLRDNORM
#endif /* #ifdef POLLRDNORM */
#ifdef POLLRDBAND
#undef POLLRDBAND
#endif /* #ifdef POLLRDBAND */
#ifdef POLLWRNORM
#undef POLLWRNORM
#endif /* #ifdef POLLWRNORM */
#ifdef POLLWRBAND
#undef POLLWRBAND
#endif /* #ifdef POLLWRBAND */
#ifdef POLLMSG
#undef POLLMSG
#endif /* #ifdef POLLMSG */

    enum Availability {
	// First 8 bits correspond to Linux bit definitions

	READ_AVAIL   = 0x0001,
	POLLIN	     = READ_AVAIL,
	URG_AVAIL    = 0x0002,
	POLLPRI	     = URG_AVAIL,
	WRITE_AVAIL  = 0x0004,
	POLLOUT	     = WRITE_AVAIL,
	EXCPT_SET    = 0x0008,
	POLLERR	     = EXCPT_SET,
	// to simplify logic, if INVALID is set, none of the other bits
	// are set, so checks for read_avail... don't have to look at INVALID
	// except in expensive case
	HUP	     = 0x0010,
	POLLHUP	     = HUP,
	INVALID      = 0x0020,		// other bits invalid
	POLLNVAL     = INVALID,
	POLLRDNORM   = 0x0040,
	POLLRDBAND   = 0x0080,
	POLLWRNORM   = 0x0100,
	POLLWRBAND   = 0x0200,
	POLLMSG	     = 0x0400,

	// These are K42 specific
	ENDOFFILE    = 0x0800, // EOF is already defined by a macro
	PENDING	     = 0x1000, // Waiting for state notification
	DESTROYED    = 0x2000, // Has been destroyed
	BACKPRES     = 0x4000, // Waiting for client action
	CLOSING	     = 0x8000, // is undergoing a close() operation
	ALL	     = 0xffff
    };
    // enum for operations that take offset
    enum At {APPEND,RELATIVE,ABSOLUTE};

    enum {EndOfFile=1};			// class "error" for read eof

    enum {SMTCutOff = 2048};  //Anything over this, use SMT
    // for setDataAvailable
    enum { SET_BITS=0, OR_BITS, CLR_BITS, AND_BITS, XOR_BITS };

    /* --------------- ASI interface ------------------- */
    /*
     * allocate a buffer of date for reading.  The buffer
     * will actually contain the values "read"
     * returns the actual length if at end of file
     */
    virtual SysStatusUval readAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatusUval readAllocAt(uval len, uval off, At at,
				      char * &bf, ThreadWait **tw);

    // indicate that the buffer is no longer needed
    virtual SysStatus readFree(char *ptr);

    // allocate an output buffer to be filled
    virtual SysStatusUval writeAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatusUval writeAllocAt(uval len, uval off, At at,
				       char * &bf, ThreadWait **tw);
    // indicate that the buffer has been written
    virtual SysStatus writeFree(char *ptr);

    // FIXME:  Should probably not allow locking to be made public
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

    // release!=0 indicates that file data cache may be cleared
    virtual SysStatus locked_flush(uval release = 0);
    /* --------------- end ASI interface ------------------- */

    virtual SysStatus flush()=0;

    virtual SysStatusUval readv(struct iovec *iov, uval vecCount,
				ThreadWait **tw, GenState &moreAvail)=0;

    virtual SysStatusUval read(char *buf, uval nbytes, ThreadWait **tw,
			       GenState &moreAvail) {
	struct iovec iov = {buf, nbytes};
	return readv(&iov, 1, tw, moreAvail);
    }

    virtual SysStatusUval writev(const struct iovec *vec, uval vecCount,
				 ThreadWait **tw, GenState &moreAvail)=0;

    virtual SysStatusUval write(const char *buf, uval nbytes,
				ThreadWait **tw, GenState &moreAvail) {
	const struct iovec iov = {(void*)buf, nbytes};
	return writev(&iov, 1, tw, moreAvail);
    }

    virtual SysStatusUval pread(const char *buf, uval nbytes, uval offset) {
	return _SERROR(2798, 0, EINVAL);
    }

    virtual SysStatusUval pwrite(const char *buf, uval nbytes, uval offset) {
	return _SERROR(2799, 0, EINVAL);
    }

    virtual SysStatusUval getDents(struct direntk42 *buf, uval len) {
	// Only directory objects should have this defined
	return _SERROR(1417, 0, ENOTDIR);
    }

    virtual SysStatus fchdir() {
	// Only directory objects should have this defined
	return _SERROR(1859, 0, ENOTDIR);
    }

    virtual SysStatusUval setFilePosition(sval position, enum At at) {
	return _SERROR(1319, 0, ESPIPE);}

    virtual SysStatus getStatus(FileLinux::Stat *status)=0;

    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	return _SERROR(1611, 0, EINVAL);
    }
    virtual SysStatus fchmod(mode_t mode) {
	return _SERROR(1317, 0, EINVAL);
    }
    virtual SysStatus ftruncate(off_t length) {
	return _SERROR(1691, 0, EINVAL);
    }

    // the way we spell close -- return 1 --> object destroyed
    virtual SysStatusUval detach();

    // claim a reference, to prevent object from disappearing out from under us
    virtual SysStatus attach();

    /*
     * For a file that can be memory mapped, returns object handle to
     * FR needed for creating specialized mappings.
     * FR type distinguishes copy on write from direct mappings
     */
    enum FRType {DEFAULT, COPYONWRITE};
    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType) {
	return _SERROR(1321, 0, EINVAL);
    }

    // Returns OH to object providing underlying implementation.
    virtual SysStatus getOH(ObjectHandle &FRoh) {
	return _SERROR(1961, 0, EINVAL);
    }

    /*
     * socket functions - these will return error for non-socket files
     */
    // unix accepts bundles a getpeername.  We don't since the
    // information is cached locally anyhow
    virtual SysStatus accept(FileLinuxRef& newSocket, ThreadWait **tw) {
	return _SERROR(1548, 0, ENOTSOCK);}

    virtual SysStatus setsockopt(uval level, uval optname,
				 const void *optval, uval optlen) {
	return _SERROR(1925, 0, ENOTSOCK);
    }

    virtual SysStatus getsockopt(uval level, uval optname,
				 const void *optval, uval *optlen) {
	return _SERROR(1925, 0, ENOTSOCK);
    }

    virtual SysStatus bind(const char* addr, uval addrlen)  {
	return _SERROR(1549, 0, ENOTSOCK);
    }

    virtual SysStatus connect(const char* addr, uval addrlen,
			      GenState &moreAvail, ThreadWait **tw) {
	return _SERROR(1550, 0, ENOTSOCK);
    }

    // this end of socket
    virtual SysStatus getsocketname(char* addr, uval &addrlen) {
	return _SERROR(1322, 0, ENOTSOCK);
    }

    // other end of socket
    virtual SysStatus getpeername(char* addr, uval &addrlen) {
	return _SERROR(1323, 0, ENOTSOCK);
    }

    virtual SysStatus listen(sval backlog)  {
	return _SERROR(1551, 0, ENOTSOCK);
    }

    virtual SysStatusUval recvfrom(char *buf, uval len, uval flags, char* addr,
				   uval &addrLen, ThreadWait **tw,
				   GenState &moreAvail) {
	struct iovec iov = {buf, len};
	struct msghdr msg = { addr, addrLen, &iov, 1, NULL, 0, 0};
	SysStatusUval rc = recvmsg(msg, flags, tw, moreAvail);
	addrLen = msg.msg_namelen;
	return rc;
    }

    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  ThreadWait **tw,
				  GenState &moreAvail) {
	return _SERROR(1552, 0, ENOTSOCK);
    }

    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				  ThreadWait **tw,
				  GenState &moreAvail) {
	return _SERROR(1553, 0, ENOTSOCK);
    }

    virtual SysStatusUval sendto(const char *buf, uval len, uval flags,
				 const char* addr,
				 uval addrLen, ThreadWait **tw,
				 GenState &moreAvail) {
	struct iovec iov = {(void*)buf, len};
	struct msghdr msg = { (void*)addr, addrLen, &iov, 1, NULL, 0, 0,};
	SysStatusUval rc = sendmsg(msg, flags, tw, moreAvail);
	return rc;
    }

    virtual SysStatus getsockname(char* addr, uval &addrLen) {
	return _SERROR(2003, 0, ENOTSOCK);
    }

    virtual SysStatus shutdown(uval how) {
	return _SERROR(2913, 0, ENOTSOCK);
    }

    static SysStatus Socket(FileLinuxRef& newSocket, sval domain, sval type,
			    sval protocol);

    static SysStatus SocketPair(FileLinuxRef& newSocket1,
				FileLinuxRef& newSocket2,
				sval domain, sval type, sval protocol);

    static SysStatus Pipe(FileLinuxRef& newPipeR, FileLinuxRef& newPipeW);

    virtual SysStatus dup(FileLinuxRef& newFile) = 0;

    // System V discretionary locks (see fcntl(2))
    virtual SysStatus getLock(struct flock &alock) {
	return _SERROR(2459, 0, EINVAL);
    }
    virtual SysStatus setLock(struct flock &alock) {
	return _SERROR(2458, 0, EINVAL);
    }
    virtual SysStatus setLockWait(struct flock &alock) {
	return _SERROR(2457, 0, EINVAL);
    }

    // Register IONotif object for notifications of state changes
    virtual SysStatus notify(IONotif *comp) {
	if (comp) {
	    comp->available.state = FileLinux::READ_AVAIL |
					FileLinux::WRITE_AVAIL;
	    comp->ready((FileLinuxRef) getRef(),
			FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL);
	}
	return 0;
    };

    // Do "notify" on kn, but, if there already is an existing object
    // on the notification list matching kn's key, then do not insert
    // on the notification list (condition check on kn will still
    // occur, and "ready()" may be called).  The already-existing
    // KeyedNotif object will be returned via "existing"
    virtual SysStatus notifyKeyed(KeyedNotif *kn, KeyedNotif* &existing) {
	return notify(kn);
    }

    virtual SysStatus getState(GenState &avail) {
	avail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;
	return 0;
    }

    // ioctl is trouble.  this represents the semantics of the linux
    // kernel call.  But its not a ppc - we will break it down on the
    // client side and do specific ppc's if needed to get the answers
    virtual SysStatus ioctl(uval request, va_list args);

    // Cannot fail

    virtual uval getFlags() {return openFlags;}
    virtual void setFlags(uval32 flags);
    virtual void modFlags(uval32 setBits, uval32 clrBits);
    virtual void clrFlags(uval32 flags);

    // FIXME: once original postFork is out of the code base, we can leave
    // this one as postFork. The method is only defined in FileLinuxStream,
    // so it can be used on crtInit.C (for objects TheConsole and TheTTY)
    virtual SysStatusUval FIXMEpostFork(ObjectHandle oh) {
	passertMsg(0, "woops\n");
	return _SERROR(1826, 0, ENOSYS);
    }

    virtual SysStatus lazyGiveAccess(sval fd) {
	passertMsg(0, "not implemented?\n");
 	return _SERROR(2541, 0, ENOSYS);
    }

    virtual void getStateTransferData(char *transferData) {
    };
    virtual uval getStateTransferDataSize() const {
	passertMsg(0, "Not implemented\n");
	return 0;
    }

    static SysStatus Create(FileLinuxRef& newFile,
			    const char* path, uval oflag, mode_t mode);

    static SysStatus Create(FileLinuxRef& newFile, TypeID clientType,
			    ObjectHandle &withOh, char *createData);

    static SysStatus Mknod(const char* nm, mode_t mode, dev_t dev);

    static SysStatus GetAbsPath(const char *nm,
				PathNameDynamic<AllocGlobal> *&pathName,
				uval &pathLen, uval &maxPathLen);

    // returns a stat struct, followLink indicates if should follo sym link
    static SysStatus GetStatus(const char *nm, FileLinux::Stat *status,
			       uval followLink = 1);

    // copies the current working directory path into a PathName
    // provided by the caller
    // returns pathLen if success
    static SysStatusUval Getcwd(
	PathNameDynamic<AllocGlobal> *pathName, uval maxPathLen);

    static SysStatus Setcwd(PathNameDynamic<AllocGlobal> *pth,
			    uval pathLen);

    // change owner on file, does not open file, caches mode info if file open
    static SysStatus Chown(const char *path, uid_t uid, gid_t gid);

    // change mode on file, does not open file, caches mode info if file open
    static SysStatus Chmod(const char *path, mode_t mode);

    // change cwd
    static SysStatus Chdir(const char *nm);

    // truncate a file to a specified length
    static SysStatus Truncate(const char *path, off_t length);

    // change file ftimes
    static SysStatus Utime(const char *path, const struct utimbuf *utbuf);

    // create a directory
    static SysStatus Mkdir(const char *path, mode_t mode);

    // removes a directory if empty
    static SysStatus Rmdir(const char *path);

    // check accessibility of file
    static SysStatus Access(const char *pathname, int mode);

    // create a hard link
    static SysStatus Link (const char *oldpath, const char* newpath);

    // deletes a name and possibly the file it refers to
    static SysStatus Unlink (const char *path);

    // change the name or location of a file
    static SysStatus Rename (const char *oldpath, const char* newpath);

    // get file system statistics
    static SysStatus Statfs(const char *path, struct statfs *buf);

    static SysStatus Sync();

    static void ClassInit();

    static uval SetUMask(uval mask) {
	uval oldMask = UMaskValue;
	UMaskValue = mask;
	return oldMask;
    }
    static uval GetUMask(void) {
	return UMaskValue;
    }

    // creates a symbolic link nm which contains path oldpath
    static SysStatus Symlink(const char *oldpath, const char *nm);

    // read a link pointed to by nm and returns buf,
    // returns length of path returned if no error
    static SysStatusUval Readlink(const char *nm, char *buf, size_t bufsize);

    //For debugging, returns the *real*, uncached state
    // Error indicates the feature is not implemented for this object
    virtual SysStatus debugAvail(GenState &raw, GenState &avail) {
	return _SERROR(2804, 0, ENOSYS);
    }

};

#endif /* #ifndef __FILE_LINUX_H_ */
