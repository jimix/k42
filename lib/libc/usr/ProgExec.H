#ifndef __ProgExec_H_
#define __ProgExec_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExec.H,v 1.93 2005/08/11 20:20:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Data structures shared between the starting process
 * and the ProgExec  code.
 * **************************************************************************/
#include <io/PathName.H>
#include <sys/ppccore.H>
#include <sys/ProcessLinux.H>
#include <sys/Dispatcher.H>
#include <scheduler/Scheduler.H>
#include <link.h>
#include <elf.h>

extern "C" uval __k42_sysinit();
extern "C" void libInitCallMain();
extern "C" void ProgExec_Init();
extern "C" void ProgExec_ForkChild();

class ProgExec {
public:
    friend void ProgExec_Init();
    friend void ProgExec_ForkChild();
    enum {
	XFERINFO_MAX_SIZE = 16 * 1024 * 1024, 	// larger than anybody will
						// ever need, but not so large
						// as to blow the paging disk
						// on erroneous calls
	WORKSPACE_SIZE = 1024 *1024 *1024,
	THREAD_COUNT = 400,
	THREAD_SIZE = (64*PAGE_SIZE),
	USR_STACK_SIZE = (512*1024*1024),
	BOOT_STACK_SIZE = (4*PAGE_SIZE),
	THREAD_STACK_RESERVATION = 0
    };
    enum {INIT_MEM_SIZE = PAGE_ROUND_UP(2*1024*1024+THREAD_COUNT*THREAD_SIZE)};


    struct ExecTypes64 {
	uval64 pointer;
	Elf64_auxv_t auxv;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr;
    };

    struct ExecTypes32 {
	uval32 pointer;
	Elf32_auxv_t auxv;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr;
    };

    //
    // What a binary module looks like
    struct BinInfo{
	uval archFlags;
	EntryPointDesc entryPointDesc;
	uval interpOffset;
	uval entryPointSpec;
	uval phdrOffset;
	uval phNum;
	enum { ANY_ADDR = ~0ULL};
	enum { MAXNUMSEG = 4 };
	struct LoadSegment{
	    uval offset;	//offset in file
	    uval vaddr;
	    uval filesz;
	    uval memsz;
	    uval flags;
	};
	uval numSegs;
	uval textSegIndex;
	uval dataSegIndex;
	LoadSegment seg[MAXNUMSEG];
	ObjectHandle frOH;
	ObjectHandle localFR;
	BinInfo *interp;
    };

    // Things we need to know in order to "exec"
    struct ExecInfo{
	BinInfo prog;
	BinInfo interp;
	BinInfo::LoadSegment stack;
	ObjectHandle stackFR;
	// These are valid only in the "exec'er"
	ObjectHandle localStackFR;

	// OH to FR for K42 library
	ObjectHandle k42FROH;

	EntryPointDesc entry;
	void localize();
    };

    struct MemRegion{
	uval start, size, offset, pageSize;
	ObjectHandle baseFROH;	// FR, if CRW, then base FR
    };

    /*
     * A few values are passed to each new VP in an initial async msg.
     */
    struct InitMsg {
	uval dispatcherStart;	// space to be used for the dispatcher
	uval dispatcherSize;	//     and early initialization

	uval xferStart;		// space that contains additional startup
	uval xferSize;		//     information (only for vp 0)
	uval xferHdr;		// location of header for startup info
    };

    struct ArgDesc{
	static SysStatus Create(const char* fileName,
				const char* const argv[],
				const char* const envp[],
				ArgDesc*&ptr);
	static SysStatus Create32(const char* fileName,
				const char* const argv[],
				const char* const envp[],
				ArgDesc*&ptr);
	void destroy(); //deletes the this pointer
	char* getFileName() {
	    return prefixDesc?((char**)(prefixDesc->args))[1]:
		((char**)args)[0]; }
	char** getArgvPrefix() {
	    return prefixDesc?prefixDesc->getArgv():0;
	}
	SysStatus setArgvPrefix(const char* const argv[]);
	SysStatus setArg0FromPath() {
	    ((char**)args)[1] = ((char**)args)[0];
	    return 0;
	}
	char** getArgv() { return (char**)args + 1; };
	char** getEnvp() { return (char**)args + 1 + argc + 1; };
	char* getArgv0() {
	    return prefixDesc?prefixDesc->getArgv0():getArgv()[0];
	}
	ProgExec::ExecInfo prog;
	uval bufSize;
	uval argc;
	uval envc;
	uval fileNameLen;
	ArgDesc* prefixDesc;
	char args[0];
    };


    /*
     * More extensive information is passed to VP 0 in a transfer area.
     * This has to be C-like, not C++-like because we need to make bit
     * by bit copies.
     */
    struct XferInfo {
	ProcessID    userProcessID;
	ProcessID    parentProcessID;
	ExecInfo     exec;

	ObjectHandle typeServerObjectHandle;
	ObjectHandle consoleOH;
	ObjectHandle processObjectHandle;
	uval debugMe;
	uval largeBrkHeap;		// set brk heap size

	char progName[64];

	// State introduced by Linux emulation
	uval umask;
	char *cwd;
	uval argc;
	char** argv;
	char** envp;

	char* execStackBase;
	ElfW(auxv_t)* auxv;
	EntryPointDesc entry;

	uval iofmSize;		// Size of file descriptor info
	uval iofmBufPtr;	// FileDescriptor information
	ObjectHandle processLinuxObjectHandle;

	/* ITEXT ... is mapping for interpreter (e.g. ld.so) is present
	 * TEXT describes the actual text region mapped, which starts
	 *      at the beginning of the file and extends at least
	 *      to the end of the text.
	 * DATA describes the region holding data.  Origin has
	 *      been rounded down and size adjusted.
	 *      origin+size is still the actual first byte of BSS
	 *      which may be in the last page of data.
	 *      offset is relevant because this region is a mapping of
	 *      a copy of the file.
	 * BSS describes the region which is appended to the end of data
	 *     to hold BSS which doesn't fit in the last page of data.
	 *     origin+size is the byte beyond the last byte of BSS,
	 *     which may be inside the last page of BSS
	 *     offset is always zero - this is a zero fill region
	 */
	enum {TEXT, DATA, BSS, ITEXT, IDATA, IBSS, KTEXT, KDATA, KBSS,
	      WORKSPACE, STACK, NUMREGIONS};
	MemRegion region[NUMREGIONS];
    };

    //
    // Put the aux vector on the stack
    //
    template<typename EXECTYPE>
    static SysStatus PutAuxVector(uval &memTop, BinInfo &info);

    template<typename EXECTYPE, class HDR>
    static SysStatus  parsePHdr(HDR *phdr, uval phdr_num, BinInfo *info);


    //
    // Parse the executable module headers located at vaddr
    //
    static SysStatus ParseExecutable(uval vaddr, ObjectHandle execFR,
				     BinInfo *info);

    //
    // Put stuff on the stack
    //
    static SysStatus SetupStack(uval stackBottomLocal, uval &stackTopLocal,
				uval &stackTop, XferInfo *info,
				ArgDesc *args);


    //
    // Define stack size and location
    //
    static SysStatus ConfigStack(ExecInfo *info);

    static XferInfo *ExecXferInfo;

    static SysStatus CreateVP(VPNum vp);
    static SysStatus CreateDispatcher(DispatcherID dspid);
    static SysStatus ForkProcess(pid_t& childLinuxPID);

private:
    static SysStatus ForkAddressSpace(uval forkInfoArg);
    friend void libInitCallMain();

public:
    class StaticLock :  public BLock {
    public:
	DEFINE_GLOBAL_NEW(StaticLock);
    };
private:
    static StaticLock * const ProgExec_ForkLock;

    struct InitInfo;
    struct ForkInfo;
    struct CreateDispatcherMsg;

    static uval MultiVP;	// flag used to inhibit fork()

    static StubTypeMgrServer *BackupTypeMgr;

    static ProcessID PID;

    static void InitPhase2(uval initInfoArg);
    static void Init(EntryPointDesc *entry = NULL);
    static void ForkWorker(uval forkInfoArg);
    static void ForkChildPhase2(uval forkInfoArg);
    static void ForkChild();
    static SysStatus LoadBinary(ExecInfo &eInfo, MemRegion *region,
				EntryPointDesc *entry);

public:
    static void LoadAndRun(ExecInfo& info, ArgDesc *args);
    static void UnMapAndExec(ArgDesc *args);

    static void RunProg(XferInfo *info);
    static SysStatus LoadAllModules(ProcessID newPID, XferInfo &xferInfo,
				    XHandle procXH, EntryPointDesc &entry);

    /*
     * Utility routines, called from RunProcessCommon(), whose kernel and
     * user-land implementations are different.  User implementations are in
     * crtInit.C.  Kernel implementations are in os/kernel/proc/crtKernel.C.
     */
    static SysStatus MapModule(
	BinInfo *info, XHandle targetXH, MemRegion *region,
	uval textType, // TEXT, ITEXT or KTEXT
	RegionType::Type keepOnExec=RegionType::FreeOnExec);

    static void ConsoleGiveAccess(ObjectHandle& ptyOH, ProcessID targetPID);
    static void ProcessLinuxGiveAccess(ObjectHandle& procLinuxOH,
				       ObjectHandle procOH);
    static SysStatus CreateFirstDispatcher(ObjectHandle childOH,
					   EntryPointDesc entry,
					   uval dispatcherAddr,
					   uval initMsgLength,
					   char *initMsg);

    /*
     * Initialization hook, called from ProgExec::InitPhase2, for
     * which the initial server launched from the kernel has an
     * implementation different from that used by all other processes.
     * The default implementation is in crtAux.C.  The
     * server-customized implementation is in
     * os/servers/baseServers/crtServer.C.
     */
    static void InitCustomizationHook(ObjectHandle processLinuxOH);
#define INCLASS
#include __MINC(ProgExecArch.H)
#undef INCLASS
};

extern EntryPointDesc SecondaryStartDesc;

extern void libInitCallMain(ProcessID parentProcessID,
			    uval iofmSize, uval iofmBufPtr,
			    char** stack_on_entry);

// FIXME: move somewhere better
extern SysStatusProcessID runExecutable(ProgExec::ArgDesc *args, uval wait);

#endif /* #ifndef __ProgExec_H_ */
