/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ConfigureArch.C,v 1.17 2005/02/16 00:06:28 mergen Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"
#include <lk/LinuxEnv.H>
#include <lk/InitCalls.H>
#include <alloc/PageAllocatorDefault.H>
extern "C" {
#include <linux/init.h>
}
#include "bilge/arch/powerpc/BootInfo.H"
#include <mem/PageAllocatorKernPinned.H>

#include "mem/RegionDefault.H"
#include "mem/FCM.H"
#include "mem/FCMStartup.H"
#include "mem/FRPlaceHolder.H"
#include <bilge/CObjGlobalsKern.H>

#include <asm/cputable.h>

struct naca_struct *naca;
extern char saved_command_line[];

extern "C" void openpic_setup_ISU(int isu_num, unsigned long addr);
extern "C" void startArch(void* ptr, uval hwData,
			  uval hwDataOffset, uval hwDataSize);

extern "C" struct obs_kernel_param* setup_ipauto;

extern "C" void HW_get_boot_time(struct rtc_time *rtc_tm);
extern "C" void HW_calibrate_decr(void);

extern "C" void pSeries_calibrate_decr(void);
void HW_calibrate_decr(void) {
    pSeries_calibrate_decr();
}

extern "C" void pSeries_get_boot_time(struct rtc_time *rtc_time);
void HW_get_boot_time(struct rtc_time *rtc_time) {
    pSeries_get_boot_time(rtc_time);
}

extern void setMemoryAllocator(PageAllocatorRef allocRef);
extern "C" void k42devfsInit();
extern struct naca_struct *naca;

int _get_PVR() {
    uval pvr;
    asm volatile ("mfpvr %0" : "=&r" (pvr));
    return pvr;
}


extern "C" void StartBootMem(void);
extern "C" void CleanUpBootMem(void);

void
ConfigureLinuxHWBase(VPNum vp)
{
    SysStatus rc;

    if (vp) return;

    RegionRef ref;
    FCMRef fcmRef;
    FRRef frRef;
    uval imageAddr;
    uval imageSize;
    uval hwData;
    uval hwDataSize;
    uval hwDataOffset;

    setMemoryAllocator((PageAllocatorRef)GOBJK(ThePinnedPageAllocatorRef));

#if 0
    tassertMsg(sizeof(spinlock_t) == sizeof(FairBLock),
	       "Lock sizes don't match between linux and K42 code\n");
#endif
    //First do some global symbols

#if 0
    tassertMsg(sizeof(struct naca_struct)==sizeof(struct LinuxNaca),
	       "Naca size out of whack\n");
#endif
    naca = (struct naca_struct*)&_BootInfo->naca;
    //tb_ticks_per_usec = Scheduler::TicksPerSecond() / 1000000;

    // Copy OF device tree image

    imageAddr = _BootInfo->hwData;
    imageSize = _BootInfo->hwDataSize;

    if (imageSize==0) {
	hwData = hwDataSize = 0;
	return;
    }

    tassert(PAGE_ROUND_DOWN(imageAddr) == imageAddr,
	    err_printf("OFImageAddr not page-aligned\n"));

    rc = FRPlaceHolder::Create(frRef);

    if (_FAILURE(rc)) goto failure;
    rc = FCMStartup::Create(fcmRef, imageAddr, imageSize);

    if (_FAILURE(rc)) goto failure;

    DREF(frRef)->installFCM(fcmRef);

    rc = RegionDefault::CreateFixedLen(
	ref, GOBJK(TheProcessRef), imageAddr,
	PAGE_ROUND_UP(_BootInfo->hwDataSize),
	PAGE_SIZE, frRef, 0, 0, AccessMode::readUserReadSup);

    if (_FAILURE(rc)) goto failure;

    rc= DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(hwData,
						    PAGE_ROUND_UP(imageSize));

    if (_FAILURE(rc)) goto failure;
    hwDataSize = imageSize;
    memcpy((void*)hwData, (void*)imageAddr, imageSize);

    DREF(ref)->destroy();

    hwDataOffset = _BootInfo->hwData;
    {
	uval pvr = _get_PVR();
	uval i = 0;
	do {
	    if ((cpu_specs[i].pvr_mask & pvr) == cpu_specs[i].pvr_value) {
		break;
	    }
	} while (cpu_specs[++i].pvr_value);

	cur_cpu_spec = &cpu_specs[i];
	(*cur_cpu_spec->cpu_setup)(0, cur_cpu_spec);
    }

    {
	LinuxEnv le(SysCall);
	StartBootMem();
#if 1
	// Mambo needs to support openpic on all platforms.

	// OF tree missing nodes that would tell Linux code to make this call
	if (KernelInfo::OnSim() && !KernelInfo::OnHV()) {
	    openpic_setup_ISU(0,0xFFC10000);
	}
#endif
	startArch(_BootInfo, hwData, hwDataOffset, hwDataSize);
	CleanUpBootMem();
    }

    return;
  failure:
    err_printf("***\n*** Failed in copying OF image\n***\n");
}

extern "C" void pmu_set_server_mode(int server_mode);
void
ConfigureLinuxHWDevArch()
{

    if (!KernelInfo::OnSim() || KernelInfo::OnHV()) {
	INITCALL(pcibios_init);

	INITCALL(of_bus_driver_init);
	if (_BootInfo->platform == PLATFORM_POWERMAC) {

	    INITCALL(adb_init);
	    INITCALL(pmac_irq_cascade_init);
	    INITCALL(pmac_late_init);
	    INITCALL(tg3_init);
	    INITCALL(gem_init);
	    INITCALL(i2c_init);
	    INITCALL(i2c_keywest_init);
	    INITCALL(macio_module_init);
	    INITCALL(macio_bus_driver_init);
//	    INITCALL(therm_pm72_init);
	    INITCALL(pmac_declare_of_platform_devices);
	    INITCALL(via_pmu_start);

	    LinuxEnv le(SysCall);
	    pmu_set_server_mode(1);

	} else {
	    //INITCALL(rpaphp_init);
	    //INITCALL(pci_hotplug_init);
	    INITCALL(pcnet32_init_module);
	    INITCALL(eeh_init_proc);
	    INITCALL(eepro100_init_module);
#ifdef HYPE
	    INITCALL(ibmveth_module_init);
#endif
	}
    }
}

void
ConfigureLinuxHWBlockArch()
{
    if (!KernelInfo::OnSim()) {
	if (_BootInfo->platform == PLATFORM_POWERMAC) {
	    INITCALL(k2_sata_init);
	} else {
	    INITCALL(spi_transport_init);
	    INITCALL(sym2_init);
	}
    } else if (KernelInfo::OnSim()==SIM_MAMBO) {
	INITCALL(mbd_init);
    }
}

extern "C" void __kickCPU(unsigned long cpu);
void KickCPU(uval cpu)
{
    LinuxEnv sc;
    __kickCPU(cpu);
}
