/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPinit.C,v 1.11 2003/01/13 19:23:19 rosnbrg Exp $
 *****************************************************************************/

// from x86 XXX

/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include "mem/PageAllocatorKern.H"
// #include "intelMPS.H"
#include "APIC.H"
#include "sys/thinwire.H"
// #include <io/IOStream.H>
#include "exception/ExceptionLocal.H"
// #include __MINC(x86.H)
#include <scheduler/Scheduler.H>
#include <mem/SegmentTable.H>
// #include <sync/MPMsgHndlr.H>
// #include <hardware.H>
#include <trace/traceBase.H>

#define inb Kinb
#define outb Koutb

//FIXME: all this stuff needs to be reorganized and cleaned up
//       just a quick hack to test mp things out with and document
//       important addresses from the MPS.

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(0x0467)
#define WARMBOOT_SEG		(0x0469)

// These are defined in secondaryStartVector.s
extern uval secondaryStartVectorSize;
extern uval secondaryStartVectorData;
extern code secondaryStartVector;
extern "C" void secondaryStart();

#define PACKED __attribute__ ((packed))

struct secondaryStartVectorData {
    uval32   procnum PACKED;
    uval32   cr3 PACKED;
    uval  stackvaddr PACKED;
    volatile uval16  startFlg PACKED;
    DTParam gdtr PACKED;
};

#ifdef CONFIG_SMP_AMD64 							/* XXX to do */

struct secondaryStartVectorData *
copySecondaryBootVector(uval ba)
{
    uval len = * ((uval *) ((uval) & secondaryStartVectorSize));
    uchar *src=(uchar *)secondaryStartVector, *dst=0;

    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(ba,(uval &)dst);
    cprintf("moving code from %lx to %lx (%ld bytes)\n",
	    src, dst, len);
    for (uval i=0; i < len; ++i)
      *dst++ = *src++;

    // FIXME:  This is just here for initial debugging
    // set the magic key
    struct secondaryStartVectorData *data=0;
    uval dataOffset = ((uval)(& secondaryStartVectorData)
			  - (uval)secondaryStartVector);
    DREFGOBJK(ThePinnedPageAllocatorRef)->
	realToVirt(ba + dataOffset,(uval &)data);
    return data;
}


void
startSecondary(const uval proc, const uval bootAddr)
{
#if 0
    // test APIC wait call
    cprintf("About to call APIC::wait(100)\n");
    APIC::wait(100);
    cprintf("After to call APIC::wait(100)!!!\n");
#endif /* #if 0 */

    // Send first INIT IPI: causes a RESET on Target Processor */

    // *** BootAddr must be a 4K page aligned address as per
    //     APIC documentation.  This Address is represented as
    //     a 1 byte vector which is sent via the local APIC
    //     to the target processors local APIC
    uchar vector = (bootAddr >> 12) & 0xff;

    cprintf("sending int to physProc %ld with apic %d\n", proc,
	    exceptionLocal.sysFacts.procId(proc));

    APIC::startSecondary(exceptionLocal.sysFacts.procId(proc),vector);
}


static uval secondaryBootMem;     //FIXME - should be vp-specific
static uval secondaryBootMemSize; //FIXME - should be vp-specific
static uval secondaryVirtBase;

void
startAll(uval numproc, uval secondaryStartVectorBaseAddr)
{

    uval mpbioswarmvec;
    uval8   mpbiosreason;
    struct secondaryStartVectorData *data;

    /*
     * 1: initialize BP's local APIC
     */
    if (!KernelInfo::OnSim()) {
	APIC::init();
    }

    /*
     * 2: initial vector to which secondary processors
     *    will jump to on Startup.  Note may want to
     *    This may be more involved later
     *    copy vector
     */
    data=copySecondaryBootVector(secondaryStartVectorBaseAddr);

    /*
     * 3: Save the current reset vector and reason
     */
    uval mpbioswarmvecptr;
    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(WARMBOOT_OFF,
						     mpbioswarmvecptr);
    mpbioswarmvec = *((uval *) mpbioswarmvecptr);
    outb(CMOS_REG, BIOS_RESET);
    mpbiosreason = inb(CMOS_DATA);

    for (uval proc=0; proc<numproc; proc++) {

	SysStatus rc;

	secondaryBootMemSize = 0x100000;
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
		allocPages(secondaryBootMem, secondaryBootMemSize);
	tassert(_SUCCESS(rc),
		    err_printf("failed to allocate secondary boot mem.\n"));
	DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(0, secondaryVirtBase);

	//  Skip all this for the Boot Processor
	if (proc == exceptionLocal.sysFacts.bootProc) continue;

	// Initialize data passed to secondary processor
	data->procnum = proc;

	// set up secondary processor to use original gdt initialized
	// at boot time
	data->gdtr.limit  = sizeof(exceptionLocal.Gdt) - 1;
	data->gdtr.base = (uval)&Gdt;
	asm volatile ("movl %%cr3, %0" : "=r" (data->cr3) :);

	rc = (DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(
	    data->stackvaddr, PAGE_SIZE));
	tassertMsg(_SUCCESS(rc), "woops\n");

	data->startFlg = 0xdead;
	cprintf("contents passed to AP are: procnum:%lx cr3:%lx "
		"stackvaddr:%lx \n \tstartFlg:%x gdtr limit:%x base:%lx\n",
		data->procnum, data->cr3, data->stackvaddr, data->startFlg,
		data->gdtr.limit, data->gdtr.base);

	// Load the secondaryStartVector base address into the bios reset
	// vector pointer and set the CMOS reset boot reason to warm boot.
	// This Ensures that the start up logic in startSecondary works
	// for "all" x86 machines.
	*((volatile uval16 *) mpbioswarmvecptr) = WARMBOOT_TARGET;
	uval mpbioswarmsegptr;
	DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(
	    WARMBOOT_SEG,mpbioswarmsegptr);
	*((volatile uval16 *) mpbioswarmsegptr) =
	    (secondaryStartVectorBaseAddr >> 4);
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

	// 6: call startSecondary (hack, will never return if sim)
	if (Scheduler::OnSim())
	    secondaryStart();

	startSecondary(proc,secondaryStartVectorBaseAddr);
	cprintf("\nThe address of the data=%lx &data->startFlg=%lx "
		"data->startFlg=%x\n",
		data, &(data->startFlg), data->startFlg);

	while ((uval16)(data->startFlg) == 0xdead) {
	    cprintf("Waiting for value at %lx (%hx) to change\n",
		    &(data->startFlg),data->startFlg);
	    APIC::wait(500000);
	}
	cprintf("secondary acked successfully\n");

    }
    // 7: Restore WARMRESET vector
    *(uval32 *) mpbioswarmvecptr = mpbioswarmvec;
    outb(CMOS_REG, BIOS_RESET);
    outb(CMOS_DATA, mpbiosreason);
}


// Harded code support for MPS 1.4 and 1.1 (as 1.4 subsumes 1.1) only
typedef struct MPS1_4_FPS MPS_FPS;
typedef struct MPS1_4_CONFIG_TABLE MPS_CONFIG_TABLE;

// Search and parse MP config info As per MPS 1.4 spec
MPS_FPS *
probeForMP()
{
    uval start=0,end=0;
    MPS_FPS *fps=0;

    cprintf("probeForMP():\n");
    // FIXME:  first look for a valid MPS1.4 FPS in top EBDA


    // Then lookin top 1k of base memory
    start=BDA_PTR_TO_BASE_MEM_SIZE;
    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(start,start);
    start=*((uval16 *)start);
    cprintf("base memory=%ld (%lx)\n",start,start);
    start=start*1024;
    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(start,start);
    end=start+1024;
    cprintf("Searching: start of start=%lx end=%lx top 1k=%x top=%x\n",
	    start, end, 639*1024, (639*1024)+1024);
    for (fps=(MPS_FPS *)start;
	  ((uval)fps)<end && !fps->Valid();
	  fps=(MPS_FPS *)((uval)fps +16))
	; // Searching

    if (((uval)fps)<end) {
	return fps;
    }

    // if not found look in  BIOS PROMS
    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(BIOS_ROM_START,start);
    DREFGOBJK(ThePinnedPageAllocatorRef)->realToVirt(BIOS_ROM_END,end);
    cprintf("Searching: start=%lx end=%lx bios_rom_start=%lx "
							"bios_rom_end=%lx\n",
	    start,end, BIOS_ROM_START, BIOS_ROM_END);
    for (fps=(MPS_FPS *)start;
	  ((uval)fps)<end && !fps->Valid();
	  fps=(MPS_FPS *)((uval)fps +16))
	;  // Searching

    if (((uval)fps)<end) {
	return fps;
    }
    return 0;
}

#endif /* #ifdef CONFIG_SMP_AMD64 */

#ifdef CONFIG_SMP_AMD64 /* XXX to do */
static SegmentTable* segmentTable;
#endif /* #ifdef CONFIG_SMP_AMD64 */

void
MPinit(uval dummy)
{
    (void) dummy;			// routines run in new threads
					// must be void (uval)
    tassert(Scheduler::GetVP() == 0,
	    err_printf("MPinit must only be called on vp 0\n"));

#ifdef CONFIG_SMP_AMD64   /* XXX to do */
    const uval secondaryBootVectorAddress=0x8000;
#endif /* #ifdef CONFIG_SMP_AMD64 */

#ifdef CONFIG_SIMICS
    cprintf("onSim()=%ld\n", KernelInfo::OnSim());
#else /* #ifdef CONFIG_SIMICS */
    xxxx
#endif /* #ifdef CONFIG_SIMICS */
#ifdef CONFIG_SMP_AMD64 							/* XXX to do */
    if (KernelInfo::OnSim()) {
	    cprintf("I am simulating an MP\n");
	    exceptionLocal.sysFacts.numProc=2;
	    exceptionLocal.sysFacts.phyProc =
		(SysFacts::physProc *)allocPinnedGlobal(2);
	    exceptionLocal.sysFacts.phyProc[0].id=0;
	    exceptionLocal.sysFacts.phyProc[1].id=1;
	    exceptionLocal.sysFacts.bootProc=0;
	    segmentTable = exceptionLocal.kernelSegmentTable;
	    startAll(exceptionLocal.sysFacts.numProc,
		     secondaryBootVectorAddress);
	    /* this never returns.  but this thread will be taken over by
	       the simulated MP logic below*/
    }
    MPS_FPS *fps=probeForMP();
    if (fps) {
	cprintf("I am an MP!!!! :)\n");
	fps->process();
	cprintf("numProc=%ld physProcs:\n",exceptionLocal.sysFacts.numProc);
	for (uval i=0; i<exceptionLocal.sysFacts.numProc; i++) {
	    cprintf("physProc[%ld]=%d\n", i,
		    exceptionLocal.sysFacts.phyProc[i].id);
	}
	startAll(exceptionLocal.sysFacts.numProc, secondaryBootVectorAddress);
#if 0
	cprintf("Before: call to int 200\n");
	asm volatile("int $200");
	cprintf("After: call to int 200\n");
#endif /* #if 0 */
    } else {
#endif /* #ifdef CONFIG_SMP_AMD64 */
	// Not an MP :(
	cprintf("I am Not an MP :(\n");
	APIC::init();
#ifdef CONFIG_SMP_AMD64 							/* XXX to do */
    }
#endif /* #ifdef CONFIG_SMP_AMD64 */
}

#ifdef CONFIG_SMP_AMD64 							/* XXX to do */

//  This is the entry point at which a
//  secondary processor enters the kernel
void
secondaryStart()
{
    KernelInitArgs kernelInitArgs;
    MemoryMgrPrimitiveKern *memory = &kernelInitArgs.memory;

    init_printf("Processor 2: am alive!!!!\n");

    // FIXME: should we be loading idtr here??
    asm("  lidt %0" : : "m" (Idtr));

#if 0
    APIC::wait(1000000);
    init_printf("Initializing my apic\n");
    APIC::init();
    init_printf("Before: call to sendIPI\n");
    APIC::sendIPI(exceptionLocal.sysFacts.procId(0),200);
    init_printf("After: call to sendIPI\n");
#endif /* #if 0 */

    uval physStart = 0;
    uval physEnd = 0x1000000;

    uval virtBase = secondaryVirtBase;

    uval allocStart = secondaryBootMem;
    uval allocEnd = allocStart + secondaryBootMemSize;

    memory->init(physStart, physEnd, virtBase, allocStart, allocEnd);

    InitKernelMappings(1, memory);

    uval onSim;
    if (thinwireTest()) {
	onSim = 1;
	init_printf("Using vps device for thinwire connection.\n");
	init_printf("Disabling VGA/KBD console.\n");
    } else {
	onSim = 0;
	init_printf("Using BIOS serial device for thinwire connection.\n");
	// with simulator kludge, interrupts are already enabled
	APIC::init();
	enableHardwareInterrupts();
    }

    kernelInitArgs.onSim = onSim;
    kernelInitArgs.vp = 1;
    kernelInitArgs.barrierP = 0;

    // FIXME: do this a better way
    exceptionLocal.sysFacts.numProc = 2;
    exceptionLocal.sysFacts.phyProc = ThePhysProcArray;

    err_printf("Proceeding with Kernel Initialization\n");

    KernelInit(kernelInitArgs);
    /* NOTREACHED */
}

extern "C" void IPCHand();
void
IPCHand()
{
    cprintf("Received IPC!!!!\n");
    asm("hlt");
}
#endif /* #ifdef CONFIG_SMP_AMD64 */
