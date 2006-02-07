/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kinit.C,v 1.127 2005/02/16 00:06:24 mergen Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include <sys/thinwire.H>
#include <bilge/arch/powerpc/openfirm.h>
#include <bilge/arch/powerpc/BootInfo.H>
#include <bilge/arch/powerpc/simos.H>
#include <exception/HWInterrupt.H>
#include "bilge/LocalConsole.H"
#include "bilge/HWPerfMon.H"
#include <scheduler/Scheduler.H>
#include <init/kernel.H>
#include <trace/traceBase.H>
#include <init/BootPrintf.H>
#include <sys/thinwire.H>
#include <sys/IOChan.H>
#include <sys/HVChan.H>
#include <sys/GDBIO.H>

BootPrintf::StaticStuff BootPrintf::staticStuff;

void writeCOM2(char c);
void writeCOM2Hex(uval x);
void writeCOM2Str(char *str);

// Defining instance of _BootInfo (declared in bilge/arch/powerpc/BootInfo.H).
struct BootInfo *_BootInfo;

// Defining instance of _OnSim (declared in kernel.H).
uval _OnSim;
uval _OnHV;

extern code kernVirtStart;

extern "C" void marctest();

extern void scanLMBForMem(MemoryMgrPrimitiveKern* memory);

/* For debugging on HV */
#if 0
extern "C" void __hcall(uval64 r3, uval64 r4, uval64 r5, uval64 r6, uval64 r7);
void print(const char* x)
{
    int j = 0;
    int l = 0;
    while (x[l]) ++l;

    while (l - j > 16) {
	__hcall(0x58ULL, 0, 16, *(uval64*)(x + j), *(uval64*)(x + j + 8));
	j += 16;
    }
    __hcall(0x58ULL, 0, l-j, *(uval64*)(x + j), *(uval64*)(x + j + 8));
}
#endif



#define _STR(x) #x
#define STR(x) _STR(x)

void
setHID() {

    if (_BootInfo->cpu_version == PV_970 ||
	_BootInfo->cpu_version == PV_970FX) {
	static uval hidInit = 0;
//      one ppc instruction per bundle
//	static uval hid0 = 0x8030100080000000ULL;
//      one ppc instruction per bundle, serialized issue
//	static uval hid0 = 0x8030180080000000ULL;

	static uval hid0 = 0x0030100080000000ULL;
	static uval hid1 = 0xfd3c200000000000ULL;
	static uval hid4 = 0ULL;
	static uval hid5 = 0ULL;

	// These sequences are what Book IV specifies
	// Note that Apple FW uses different settings, in particular
	// for HID5 which changes dcbz size to 32 instead of 128
	asm volatile(
	    "cmpdi	%4, 0\n\t"
	    "bne	1f\n\t"
	    "mfspr	r9, " STR(SPRN_HID0) "\n\t"
	    "or		%0, %0, r9\n\t"
	    "mfspr	r9, " STR(SPRN_HID1) "\n\t"
	    "or		%1, %1, r9\n\t"
	    "mfspr	r9, " STR(SPRN_HID4) "\n\t"
	    "or		%2, %2, r9\n\t"
	    "mfspr	r9, " STR(SPRN_HID5) "\n\t"
	    "or		%3, %3, r9\n\t"
	    "1:\n\t"
	    "sync\n\t"
	    "mtspr	" STR(SPRN_HID0) ",%0\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "mfspr	%0, " STR(SPRN_HID0) "\n\t"
	    "isync\n\t"
	    "sync\n\t"
	    "mtspr	" STR(SPRN_HID1) ",%1\n\t"
	    "mtspr	" STR(SPRN_HID1) ",%1\n\t"
	    "isync\n\t"
	    "sync\n\t"
	    "mtspr	" STR(SPRN_HID4) ",%2\n\t"
	    "isync\n\t"
	    "sync\n\t"
	    "mtspr	" STR(SPRN_HID5) ",%3\n\t"
	    "isync\n\t"
	    : "=&r"(hid0), "=&r"(hid1), "=&r"(hid4), "=&r"(hid5)
	    : "0"(hid0), "1"(hid1), "2"(hid4), "3"(hid5), "r"(hidInit)
	    :"r9");
	hidInit = 1;
    }
}
extern uval FIXME_LOGPTES;

extern "C" void
start(BootInfo *bootInfo)
{
    /*
     * We don't want to rely on the boot program to install the MSR we want,
     * so set it ourselves.
     */
    asm volatile ("mtmsrd %0; isync" : : "r" (PSL_KERNELSET&(~PSL_ISF)));

    /*
     * Initialize CurrentThread register to catch early bogus uses.
     */
    asm volatile("mr r13,%0" : : "r" (0xdeaddeaddeadbeef) : "r13");

    /*
     * Initialize all the floating point registers.
     */
    float tmp __attribute__((unused))= 0.0;
    asm volatile (
	"lfd f0,%0\n\t"
	"fmr f1,f0\n\t"
	"fmr f2,f0\n\t"
	"fmr f3,f0\n\t"
	"fmr f4,f0\n\t"
	"fmr f5,f0\n\t"
	"fmr f6,f0\n\t"
	"fmr f7,f0\n\t"
	"fmr f8,f0\n\t"
	"fmr f9,f0\n\t"
	"fmr f10,f0\n\t"
	"fmr f11,f0\n\t"
	"fmr f12,f0\n\t"
	"fmr f13,f0\n\t"
	"fmr f14,f0\n\t"
	"fmr f15,f0\n\t"
	"fmr f16,f0\n\t"
	"fmr f17,f0\n\t"
	"fmr f18,f0\n\t"
	"fmr f19,f0\n\t"
	"fmr f20,f0\n\t"
	"fmr f21,f0\n\t"
	"fmr f22,f0\n\t"
	"fmr f23,f0\n\t"
	"fmr f24,f0\n\t"
	"fmr f25,f0\n\t"
	"fmr f26,f0\n\t"
	"fmr f27,f0\n\t"
	"fmr f28,f0\n\t"
	"fmr f29,f0\n\t"
	"fmr f30,f0\n\t"
	"fmr f31,f0\n\t"
	"mtfsf 0xff,f0\n\t" : : "m" (tmp));

    struct KernelInitArgs kernelInitArgs;
    MemoryMgrPrimitiveKern *memory = &kernelInitArgs.memory;
    _BootInfo = bootInfo;
    _OnSim = _BootInfo->onSim;
    _OnHV  = _BootInfo->onHV;

    if (!_OnHV) setHID();

    kernelInitArgs.vp = 0;
    kernelInitArgs.barrierP = 0;
    kernelInitArgs.sharedIPT = 0;

    /*
     * We want to map all of physical memory into a V->R region.  We choose a
     * base for the V->R region (virtBase) that makes the kernel land correctly
     * at its link origin, &kernVirtStart.  This link origin must wind up
     * mapped to the physical location at which the kernel was loaded
     * (_BootInfo->kernelImage).
     */
    uval virtBase = (uval) (&kernVirtStart - _BootInfo->kernelImage);

    if (_BootInfo->platform & PLATFORM_PSERIES) {
	rtas_init(virtBase);
    }

    /*
     * Not all of physical memory is addressable yet.  The boot program maps
     * some amount of memory beyond the kernel which we can use as an initial
     * page pool for early allocation.
     */
    extern int _end[];
    uval allocStart = PAGE_ROUND_UP(uval(_end));
    uval allocEnd = PAGE_ROUND_DOWN(virtBase + _BootInfo->kernelImage +
						_BootInfo->kernelImageSize);

    uval physStart = 0;
    uval physEnd = _BootInfo->physEnd;

    memory->init(physStart, physEnd, virtBase, allocStart, allocEnd);

    BootPrintf::Init(memory);

    BootPrintf::Printf("MemoryMgrPrimitive allocStart %lx allocEnd %lx\n",
		       allocStart, allocEnd);


    //Read the LMB structures to figure out what memory should be
    //given to the allocator. allocEnd tells this code where the free
    //memory really begins.  The LMB may be out of date, due to a
    //download of a new kernel.
    scanLMBForMem(memory);


    HWInterrupt::Init();

    if (_BootInfo->platform == PLATFORM_PSERIES) {
	/*
	 * Initialize Performance Monitoring Hardware to a known state
	 */
	HWPerfMon::VPInit();
    }

    InvertedPageTable::ClassInit(memory);

    InitKernelMappings(kernelInitArgs);

    /*
     * Hardware interrupts have been disabled up this point.  We enable now
     * to avoid assertions in subsequent boot code.  We're not yet ready for
     * timer or external interrupts, so null handlers have been established
     * for them.
     */
    enableHardwareInterrupts();

    if (_OnSim && !_OnHV) simosThinIPInit(memory);
    ThinWireChan::ClassInit(0, memory);

    {
	IOChan* console = NULL;
	IOChan* gdb = NULL;
	uval useVty = 0;
	if ((useVty || KernelInfo::OnSim()) && KernelInfo::OnHV()) {
	    console = new(memory) HVChannel(CONSOLE_CHANNEL);
	    gdb = new(memory) HVChannel(GDB_CHANNEL);
	} else if (KernelInfo::OnSim()) {
	    console = getSimConChan(memory);
	} else {
	    /* hardware only uses thinwire */
	    console = new(memory) ThinWireChan(CONSOLE_CHANNEL);
	    gdb = new(memory) ThinWireChan(GDB_CHANNEL);
	}


	GDBIO::GDBChan = gdb;
	LocalConsole::Init(0, memory, console);
    }

    BootPrintf::PrintAll(memory);
    err_printf("Calling KernelInit.C (first real printf) %p\n", _BootInfo);
    err_printf("Initial page pool: %lx %lx\n",allocStart, allocEnd);
    KernelInit(kernelInitArgs);
    /* NOTREACHED */
}
