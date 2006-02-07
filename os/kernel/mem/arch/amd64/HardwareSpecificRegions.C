/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HardwareSpecificRegions.C,v 1.9 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentTable.H"
#include "mem/HardwareSpecificRegions.H"
#include "mem/FCMFixed.H"
#include "mem/RegionDefault.H"

extern "C"  void early_printk(const char *fmt, ...);
#define APIC_DEFAULT_PHYS_BASE  	(0xfee00000)

uval HardwareSpecificRegions::APICVAddr=0;
uval HardwareSpecificRegions::isInitialized=0;

/* static */ uval
HardwareSpecificRegions::GetAPICVaddr()
{
    tassertMsg(!KernelInfo::OnSim(), "woops\n");
    return APICVAddr;
}

/* static */ uval
HardwareSpecificRegions::IsInitialized()
{
    return isInitialized;
}

static void
MapUncache(SegmentTable *pageDirectory, uval virtaddr, uval size)
{
    uval v;

    for (v = virtaddr; v < (virtaddr + size); v += PAGE_SIZE) {
        PML4 *pml4_p = VADDR_TO_PML4_P(pageDirectory,virtaddr);
	if(pml4_p == ( PML4 *)NULL || pml4_p->P == 0) {
            early_printk("ERROR<%s,%d>: No page table\n",
                        __FILE__, __LINE__ );
            while(1){}
        }
        else
	    pml4_p->US = 1;

        PDP *pdp_p = VADDR_TO_PDP_P(pageDirectory,virtaddr);
	if(pdp_p->P == 0) {
            early_printk("ERROR<%s,%d>: No page table\n",
                        __FILE__, __LINE__ );
            while(1){}
        }
        else
	    pdp_p->US = 1;

        PDE *pde_p = VADDR_TO_PDE_P(pageDirectory,virtaddr);
	if(pde_p->P == 0 || pde_p->PS) {
            early_printk("ERROR<%s,%d>: No page table or 2MB page size\n",
                        __FILE__, __LINE__ );
            while(1){}
        }
        else
	    pde_p->US = 1;

        PTE *pte_p = VADDR_TO_PTE_P(pageDirectory,virtaddr);
	if(pte_p->P == 0) {
            early_printk("ERROR<%s,%d>: No page table\n",
                        __FILE__, __LINE__ );
            while(1){}
        }
        else
	    pte_p->PCD = 1;

    }
}

void HardwareSpecificRegions::ClassInit(VPNum vp)
{
    if (KernelInfo::OnSim()) return;
    if (vp!=0) return;

    isInitialized = 1;
    // create FCM
    FRRef fr = NULL;
    SysStatus rc;

    breakpoint();  // following code needs to be revised
#ifdef CHANGE_FCM_TO_FRR
    rc = FCMFixed<AllocPinnedGlobalPadded>::Create(fr);
    tassertMsg( _SUCCESS(rc), "woops\n");
    DREF(fr)->establishPagePhysical(0, APIC_DEFAULT_PHYS_BASE, PAGE_SIZE);
#endif /* #ifdef CHANGE_FCM_TO_FRR */

    RegionRef regionRef;
    uval vaddr;
    // bind in a new region, mapped by this FCM
    rc = RegionDefaultKernel::CreateFixedLen(
	regionRef, GOBJK(TheProcessRef), vaddr,
	PAGE_SIZE, 0, fr, 0, AccessMode::noUserWriteSup);
    tassertMsg( _SUCCESS(rc), "woops\n");
    /*
     * map in uncached
     */
    MapUncache(exceptionLocal.kernelSegmentTable, vaddr, PAGE_SIZE);

    // assign global address for APICVAddr
    HardwareSpecificRegions::APICVAddr = vaddr;
    err_printf("allocated for APIC address %lx\n", vaddr);
}
