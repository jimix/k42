/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelExit.C,v 1.34 2005/02/16 00:06:24 mergen Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *		KernelExit(), which on powerpc reboots from a preserved
 *		kernelimage.
 * **************************************************************************/

#include <kernIncs.H>
#include <init/kernel.H>
#include <sys/baseBootServers.H>
#include <sys/thinwire.H>
#include <mem/PageAllocatorKernPinned.H>
#include <bilge/arch/powerpc/BootInfo.H>
#include <bilge/arch/powerpc/openfirm.h>
#include <bilge/arch/powerpc/simos.H>
#include <exception/ExceptionLocal.H>
#include <exception/DispatcherDefaultKern.H>

extern bootServerHeader bootServers[];

extern "C" void GdbExit(sval sigval);
extern "C" void ConfigureLinuxShutDown();

struct SegInfo {
    uval64 pStart, vStart, size, filePtr;
};

struct ParseInfo {
    struct SegInfo text;
    struct SegInfo data;
    struct SegInfo bss;
    uval64 *entryFunc;
};
static int ParseElf(uval64 kernStartAddr, struct ParseInfo *pi);



static void Reboot(uval killThinwire)
{
    VPNum ppCount, pp, hwpp;
    bootServerHeader *hdr;
    uval imageVirt, imageSize;
    uval p;

    pp = kernelInfoLocal.physProc;
    if (pp == 0) {
	// master processor

	// wait for secondary processors
	ppCount = _SGETUVAL(DREFGOBJ(TheProcessRef)->vpCount());
	for (pp = 1; pp < ppCount; pp++) {
	    hwpp = HWInterrupt::PhysCPU(pp);
	    volatile uval64 *syncPtr = &_BootInfo->startCPU[hwpp].startIAR;
	    while ((*syncPtr) != 0) {
		/* spin */;
	    }
	}

	err_printf("KernelExit: muzzling interrupt controller.\n");
	if (exceptionLocal.hwInterrupt) {
	    exceptionLocal.hwInterrupt->CPUDead(1);
	}

	err_printf("KernelExit: closing GDB.\n");
	GdbExit(9);

	for (hdr = bootServers;
	     (hdr->offset() != 0) && (strcmp(hdr->name(), "reboot") != 0);
	     hdr++)
	{
	    /*no-op*/;
	}

	if (hdr->offset() == 0) {
	    cprintf("KernelExit:  \"reboot\" not found in bootServers.\n");
	    return;
	}

	imageVirt = ((uval) bootServers) + hdr->offset();
	imageSize = PAGE_ROUND_UP(hdr->size());

	ParseInfo pi;
	ParseElf(imageVirt, &pi);

	err_printf("KernelExit: copying reboot program.\n");

	err_printf("Text: vaddr 0x%016llX   paddr 0x%016llX\n"
		   "       size 0x%016llX  offset 0x%016llX\n",
		   pi.text.vStart, pi.text.pStart, pi.text.size,
		   pi.text.filePtr);

	memcpy((void *) PageAllocatorKernPinned::realToVirt(pi.text.vStart),
	       (void*)pi.text.filePtr, pi.text.size);

	err_printf("Data: vaddr 0x%016llX   paddr 0x%016llX\n"
		   "       size 0x%016llX  offset 0x%016llX\n",
		   pi.data.vStart, pi.data.pStart, pi.data.size,
		   pi.data.filePtr);

	memcpy((void *) PageAllocatorKernPinned::realToVirt(pi.data.vStart),
	       (void*)pi.data.filePtr, pi.data.size);

	err_printf("BSS:  vaddr 0x%016llX   paddr 0x%016llX \n"
		   "       size 0x%016llX\n",
		   pi.bss.vStart, pi.bss.pStart, pi.bss.size);
	memset((void *) PageAllocatorKernPinned::realToVirt(pi.bss.vStart),
	       0, pi.bss.size);


	err_printf("KernelExit: synchronizing caches.\n");

	uval imageStart = PageAllocatorKernPinned::realToVirt(pi.text.vStart);
	uval imageEnd = PageAllocatorKernPinned::realToVirt(pi.bss.vStart +
							    pi.bss.size);

	err_printf("Cache sync: 0x%016lx - 0x%016lx %x\n",
		   imageStart, imageEnd, _BootInfo->dCacheL1LineSize);
	asm volatile ("sync");
	for (p = imageStart; p < (imageEnd);
	     p += 128/*_BootInfo->dCacheL1LineSize*/) {
	    asm volatile ("dcbst 0,%0; icbi 0,%0" : : "r" (p));
	}
	asm volatile ("isync");

	imageEnd = PageAllocatorKernPinned::virtToReal(imageEnd);

	err_printf("KernelExit: launching ... iar:%llx sp:%lx info:%lx\n",
		   pi.entryFunc[0], imageEnd + 20*PAGE_SIZE,
		   PageAllocatorKernPinned::virtToReal(uval(_BootInfo)));

	if (killThinwire) {
	    ThinWireChan::thinwireExit();
	}


	asm volatile ("\n"
	    "	    b	KernelExitAligned   # get to aligned boundary	\n"
	    "	    .align	5					\n"
	    "	KernelExitAligned:					\n"
	    "	    mtsrr0	%0    # no mapping faults from here on	\n"
	    "	    mtsrr1	%1					\n"
	    "	    mr	r1,%2						\n"
	    "	    mr	r2,%3						\n"
	    "	    mr	r3,%4						\n"
	    "	    rfid						\n"
	    :
	    :   "r" (pi.entryFunc[0]),
		"r" (PSL_HV | PSL_ME),
		"r" (imageEnd + 20*PAGE_SIZE),
		"r" (pi.entryFunc[1]),
		"r" (PageAllocatorKernPinned::virtToReal(uval(_BootInfo)))
	    : "r3"
	);
    } else {
	// secondary processor
	hwpp = HWInterrupt::PhysCPU(pp);
	asm volatile ("\n"
	    "	    b	RebootMsgAligned    # get to aligned boundary	\n"
	    "	    .align	4					\n"
	    "	RebootMsgAligned:					\n"
	    "	    mtsrr0	%0    # no mapping faults from here on	\n"
	    "	    mtsrr1	%1					\n"
	    "	    mr		r3,%2					\n"
	    "	    rfid						\n"
	    :
	    :	"r" (PageAllocatorKernPinned::
			    virtToReal(uval(&_BootInfo->spinLoopCode))),
		"r" (PSL_SF | PSL_HV | PSL_ME),
		"r" (PageAllocatorKernPinned::
			    virtToReal(uval(&_BootInfo->startCPU[hwpp])))
	    : "r3"
	);
    }
}

struct RebootMsg : MPMsgMgr::MsgAsync {
    uval killThinwire;
    virtual void handle() {
	uval const myKillTW = killThinwire; // copy data before freeing msg
	free();
	if (exceptionLocal.hwInterrupt) {
	    exceptionLocal.hwInterrupt->CPUDead(0);
	}
	disableHardwareInterrupts();
	Reboot(myKillTW);
    }
};

/*
 * This boolean can be toggled from the debugger.  It will cause a "kill"
 * command to generate a hard reboot rather than attempt a fast reboot.  It's
 * useful when the kernel is in the debugger and it's known to be in a state
 * in which a fast reboot will not work.  It can sometimes save a trip over
 * to the lab to reboot kitch13 manually.
 */
static uval ForceHardReboot = 0;
#include <bilge/arch/powerpc/openfirm.h>

void
KernelExit(uval killThinwire, uval physProcs, uval ctrlFlags)
{
    SysStatus rc;
    VPNum ppCount, pp, hwpp;

    if (KernelInfo::OnSim()) {
	// whatever we wish simos to return when it exits
	uval code = 0;
	SimOSSupport(SimExitCode, code);
	// NOTREACHED
	return;
    }

    if (ForceHardReboot) {
	//rtas_system_reboot();
    }

    /*
     * This code might be called from the debugger stub, in which case we're
     * on the debugger stack, and we're hardware-disabled.  On other paths we
     * may be enabled and on a thread, but for consistency's sake we choose
     * to always run disabled.
     */
    if (hardwareInterruptsEnabled()) {
	disableHardwareInterrupts();
    }

    /*
     * We may be running at exception level in some other process's context.
     * Some of the subsequent code ought run on a thread, but it has at least
     * some chance of working if we're in the kernel context.
     */
    if (exceptionLocal.currentProcessAnnex !=
		exceptionLocal.kernelProcessAnnex) {
	exceptionLocal.kernelProcessAnnex->switchContextKernel();
    }

    ConfigureLinuxShutDown();

    /*
     * It's really not correct to send MP messages while hardware-disabled,
     * but it will work unless we block waiting for a message buffer.  If it
     * turns out to be a problem, we'll have to resort to a more primitive
     * signaling mechanism.
     */
    err_printf("KernelExit: signaling other processors:\n");
    ppCount = KernelInfo::MaxPhysProcs();
    for (pp = 0; pp < ppCount; pp++) {
	hwpp = HWInterrupt::PhysCPU(pp);
	err_printf("                processor %ld (phys %ld) ... ", pp, hwpp);
	if (pp == kernelInfoLocal.physProc) {
	    err_printf("self\n");
	} else if ((pp != 0) && (_BootInfo->startCPU[hwpp].startIAR == 0)) {
	    err_printf("already spinning\n");
	} else {
	    RebootMsg *const msg =
		new(DISPATCHER_KERN->getEnabledMsgMgr()) RebootMsg;
	    msg->killThinwire = killThinwire;
	    rc = msg->send(SysTypes::DSPID(0, pp));
	    if (!_SUCCESS(rc)) {
		cprintf("KernelExit:  send to processor %ld failed.\n", pp);
		return;
	    }
	    err_printf("signaled\n");
	}
    }

    _BootInfo->processorCount = physProcs;
    _BootInfo->controlFlags = ctrlFlags;

    Reboot(killThinwire);
}

static int
ParseElf(uval64 imageAddr, struct ParseInfo *pi)
{
    struct elf64_hdr {
	uval8  e_ident[16];
	sval16 e_type;
	uval16 e_machine;
	sval32 e_version;
	uval64 e_entry;
	uval64 e_phoff;
	uval64 e_shoff;
	sval32 e_flags;
	sval16 e_ehsize;
	sval16 e_phentsize;
	sval16 e_phnum;
	sval16 e_shentsize;
	sval16 e_shnum;
	sval16 e_shstrndx;
    };
    struct elf64_phdr {
	sval32 p_type;
	sval32 p_flags;
	uval64 p_offset;
	uval64 p_vaddr;
	uval64 p_paddr;
	uval64 p_filesz;
	uval64 p_memsz;
	uval64 p_align;
    };

    enum {
	PF_X = (1 << 0),
	PF_W = (1 << 1),
	PF_R = (1 << 2),
	PF_ALL = (PF_X | PF_W | PF_R),
    };

    enum {
	PT_LOAD = 1,
	PT_GNU_STACK = 0x60000000 + 0x474e551
    };

    struct elf64_hdr *e64 = (struct elf64_hdr *)imageAddr;
    struct elf64_phdr *phdr;
    int i;
    if ((int)e64->e_ident[0] != 0x7f
	|| e64->e_ident[1] != 'E'
	|| e64->e_ident[2] != 'L'
	|| e64->e_ident[3] != 'F'
	|| e64->e_machine != 21 /* EM_PPC64 */) {
	err_printf("Bad header: %x %c%c%c %d\n",
	       e64->e_ident[0], e64->e_ident[1],
	       e64->e_ident[2], e64->e_ident[3],
	       e64->e_machine);
	return (0);
    }
    phdr = (struct elf64_phdr *)((uval)e64 + e64->e_phoff);

    // We know that the order is text, data that includes bss
    if (e64->e_phnum > 3) {
	err_printf("ERROR: too many ELF segments %lld",
	       (uval64) e64->e_phnum);
	return 0;
    }

    /* FIXME: This is cheeesy */
    for (i = 0; i < e64->e_phnum; i++) {
	if (phdr[i].p_type == PT_LOAD) {
	    if (phdr[i].p_flags == (PF_R | PF_X) ||
		phdr[i].p_flags == PF_ALL) {
		pi->text.filePtr  = imageAddr + phdr[i].p_offset;
		pi->text.size = phdr[i].p_filesz;
		pi->text.vStart  = phdr[i].p_vaddr;
		if (phdr[i].p_memsz != pi->text.size) {
		    err_printf("ERROR: text memsz and text filesz are not equal");
		    return 0;
		}
	    } else if (phdr[i].p_flags == (PF_R | PF_W)) {
		pi->data.filePtr = imageAddr + phdr[i].p_offset;
		pi->data.size = phdr[i].p_filesz;
		pi->data.pStart = pi->data.vStart  = phdr[i].p_vaddr;

		// bss
		pi->bss.filePtr = 0;
		pi->bss.size = phdr[i].p_memsz - phdr[i].p_filesz;
		pi->data.pStart = pi->bss.vStart =
		    phdr[i].p_vaddr + phdr[i].p_filesz;

		// entry is a function descriptor
		if (e64->e_entry >= phdr[i].p_vaddr &&
		    e64->e_entry <= phdr[i].p_vaddr +
		    phdr[i].p_memsz) {

		    pi->entryFunc = (uval64 *)
			(pi->data.filePtr + e64->e_entry - phdr[i].p_vaddr);
		    err_printf("Entry desc: %llx %llx [%llx,%llx]\n\r",
			       e64->e_entry,
			       (uval64)pi->entryFunc,
			       (uval64)pi->entryFunc[0],
			       (uval64)pi->entryFunc[1]);
		} else {
		    err_printf("ERROR: entry descriptor is not in data segment");
		    return 0;
		}
	    }
	} else if (phdr[i].p_type == PT_GNU_STACK) {
	    /* We have a GNU_STACK segment - if we want to implement NX pages,
	     * turn on here */

	}
    }
    return 1;
}
