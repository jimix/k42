/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PerfMon.C,v 1.20 2001/04/20 14:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/

#include <kernIncs.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>

#include "bilge/PerfMon.H"
#include "bilge/arch/powerpc/BootInfo.H"

/* static */
PerfMon::Events PerfMon::measure=PerfMon::NONE;

SysStatus
PerfMon::GetMMCR0(MMCR0 *mmcr0)
{
    if (_BootInfo->cpu_version == VER_APACHE) {
	__asm __volatile("mfspr %0, 141" : "=r"((*mmcr0).mmcr0_old));
	return 0;
    }
    else if (_BootInfo->cpu_version == VER_630) {
	__asm __volatile("mfspr %0, 779" : "=r"((*mmcr0).mmcr0_new));
	return 0;
    }
    else
	return -1;
}

SysStatus
PerfMon::SetMMCR0(MMCR0 mmcr0)
{
    if (_BootInfo->cpu_version == VER_APACHE) {
	__asm __volatile("mtspr 157, %0" : : "r"(mmcr0.mmcr0_old));
	return 0;
    }
    else if (_BootInfo->cpu_version == VER_630) {
	__asm __volatile("mtspr 795, %0" : : "r"(mmcr0.mmcr0_new));
	return 0;
    }
    else
	return -1;
}

SysStatus
PerfMon::GetMMCR1(MMCR1 *mmcr1)
{
    if (_BootInfo->cpu_version == VER_APACHE)
	return -1;
    else if (_BootInfo->cpu_version == VER_630) {
	__asm __volatile("mfspr %0, 782" : "=r"(*mmcr1));
	return 0;
    }
    else
	return -1;
}

SysStatus
PerfMon::SetMMCR1(MMCR1 mmcr1)
{
    if (_BootInfo->cpu_version == VER_APACHE)
	return -1;
    else if (_BootInfo->cpu_version == VER_630) {
	__asm __volatile("mtspr 798, %0" : : "r"(mmcr1));
	return 0;
    }
    else
	return -1;
}

SysStatus
PerfMon::GetPMC(uval pmc, uval32 *value)
{
    switch (pmc) {
    default:
	return -1;
    case 1:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mfspr %0, 137" : "=r" (*value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 771" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 2:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mfspr %0, 138" : "=r" (*value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 772" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 3:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mfspr %0, 139" : "=r" (*value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 773" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 4:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mfspr %0, 140" : "=r" (*value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 774" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 5:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 775" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 6:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 776" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 7:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 777" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 8:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mfspr %0, 778" : "=r" (*value));
	    return 0;
	}
	else
	    return -1;
	break;
    }
}

SysStatus
PerfMon::SetPMC(uval pmc, uval32 value)
{
    switch (pmc) {
    default:
	return -1;
    case 1:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mtspr 153, %0" : : "r" (value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 787, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 2:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mtspr 154, %0" : : "r" (value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 788, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 3:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mtspr 155, %0" : : "r" (value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 789, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 4:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    __asm __volatile("mtspr 156, %0" : : "r" (value));
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 790, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 5:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 791, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 6:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 792, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 7:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 793, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    case 8:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    __asm __volatile("mtspr 794, %0" : : "r" (value));
	    return 0;
	}
	else
	    return -1;
	break;
    }
}

SysStatus
PerfMon::Select(uval pmc, uval32 value)
{
    MMCR0 mmcr0;
    MMCR1 mmcr1;

    switch (pmc) {
    default:
	return -1;
    case 1:
	GetMMCR0(&mmcr0);
	if (_BootInfo->cpu_version == VER_APACHE)
	    mmcr0.mmcr0_old.pmc1 = value;
	else if (_BootInfo->cpu_version == VER_630)
	    mmcr0.mmcr0_new.pmc1 = value;
	else
	    return -1;
	SetMMCR0(mmcr0);
	return 0;
	break;
    case 2:
	GetMMCR0(&mmcr0);
	if (_BootInfo->cpu_version == VER_APACHE)
	    mmcr0.mmcr0_old.pmc2 = value;
	else if (_BootInfo->cpu_version == VER_630)
	    mmcr0.mmcr0_new.pmc2 = value;
	else
	    return -1;
	SetMMCR0(mmcr0);
	return 0;
	break;
    case 3:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    GetMMCR0(&mmcr0);
	    mmcr0.mmcr0_old.pmc3 = value;
	    SetMMCR0(mmcr0);
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc3 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    case 4:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    GetMMCR0(&mmcr0);
	    mmcr0.mmcr0_old.pmc4 = value;
	    SetMMCR0(mmcr0);
	    return 0;
	}
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc4 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    case 5:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc5 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    case 6:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc6 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    case 7:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc7 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    case 8:
	if (_BootInfo->cpu_version == VER_APACHE)
	    return -1;
	else if (_BootInfo->cpu_version == VER_630) {
	    GetMMCR1(&mmcr1);
	    mmcr1.pmc8 = value;
	    SetMMCR1(mmcr1);
	    return 0;
	}
	else
	    return -1;
	break;
    }
}

void
PerfMon::Stop()
{
    if (KernelInfo::OnSim())
	return;

    MMCR0 mmcr0;
    GetMMCR0(&mmcr0);
    if (_BootInfo->cpu_version == VER_APACHE)
	mmcr0.mmcr0_old.disable = 1;
    else if (_BootInfo->cpu_version == VER_630)
	mmcr0.mmcr0_new.disable = 1;
    SetMMCR0(mmcr0);
    return;
}

void
PerfMon::Start()
{
    if (KernelInfo::OnSim())
	return;

    if (PerfMon::measure == PerfMon::NONE)
	return;

    MMCR0 mmcr0;
    GetMMCR0(&mmcr0);
    if (_BootInfo->cpu_version == VER_APACHE)
	mmcr0.mmcr0_old.disable = 0;
    else if (_BootInfo->cpu_version == VER_630)
	mmcr0.mmcr0_new.disable = 0;
    SetMMCR0(mmcr0);
    return;
}

void
PerfMon::Collect()
{
    if (KernelInfo::OnSim())
	return;

    PerfMon::Stop();
    PerfMon::Zero();

    if (_BootInfo->cpu_version == VER_APACHE) {
	MMCR0 mmcr0;
	GetMMCR0(&mmcr0);
	mmcr0.mmcr0_old.interrupt = 1; /* Disable interrupts */
	SetMMCR0(mmcr0);
    }

    switch (PerfMon::measure) {
    default:
	err_printf("Invalid Performance Monitor measurement\n");
	break;
    case NONE:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    PerfMon::Select(1, 0x0);
	    PerfMon::Select(2, 0x0);
	    PerfMon::Select(3, 0x0);
	    PerfMon::Select(4, 0x0);
	} else if (_BootInfo->cpu_version == VER_630) {
	    PerfMon::Select(1, 0);
	    PerfMon::Select(2, 0);
	    PerfMon::Select(3, 0);
	    PerfMon::Select(4, 0);
	    PerfMon::Select(5, 0);
	    PerfMon::Select(6, 0);
	    PerfMon::Select(7, 0);
	    PerfMon::Select(8, 0);
	}
	break;
    case BUS:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    PerfMon::Select(1, 0x40); /* Addr Bus Total */
	    PerfMon::Select(2, 0x44); /* Addr Bus Local */
	    PerfMon::Select(3, 0x44); /* Data Bus Total */
	    PerfMon::Select(4, 0x45); /* Data Bus Local */
	} else if (_BootInfo->cpu_version == VER_630) {
	    PerfMon::Select(1, 0);  /* Processor Cycles */
	    PerfMon::Select(2, 0);  /* Instructions Completed */
	    PerfMon::Select(4, 20); /* Bus ARespIn Retry */
	}
	break;
    case DCACHE:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    PerfMon::Select(1, 0xC);  /* DCache Misses */
	    PerfMon::Select(2, 0x2);  /* Storage Delay Cycles */
	    PerfMon::Select(3, 0xC);  /* DCache Stall Cycles */
	    PerfMon::Select(4, 0x3A); /* DCache Invalidates */
	} else if (_BootInfo->cpu_version == VER_630) {
	    PerfMon::Select(1, 0);  /* Processor Cycles */
	    PerfMon::Select(2, 0);  /* Instructions Completed */
	    PerfMon::Select(3, 5);  /* L1 Load Miss */
	    PerfMon::Select(4, 27); /* L2 M->E or S */
	    PerfMon::Select(5, 10); /* L2 M->I */
	    PerfMon::Select(6, 13); /* DCache Prefetch Buffer Invalidates */
	}
	break;
    case INSN:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    PerfMon::Select(1, 0x6B); /* Cumulative Sync Pipeline Stalls */
	    PerfMon::Select(2, 0x15); /* FXU insns */
	    PerfMon::Select(3, 0x1);  /* Processor cycles */
	    PerfMon::Select(4, 0x3);  /* Instructions Executed */
	} else if (_BootInfo->cpu_version == VER_630) {
	    PerfMon::Select(1, 0);  /* Processor Cycles */
	    PerfMon::Select(2, 0);  /* Instructions Completed */
	    PerfMon::Select(3, 18); /* Sync rerun ops */
	}
	break;
    case TLB:
	if (_BootInfo->cpu_version == VER_APACHE) {
	    PerfMon::Select(1, 0xE);  /* Cumulative DTLB Miss Duration */
	    PerfMon::Select(2, 0x4);  /* TLBIE Received All */
	    PerfMon::Select(3, 0x5);  /* TLBIE Received Local */
	    PerfMon::Select(4, 0x6);  /* Total DTLB TLBIE Hits */
	} else if (_BootInfo->cpu_version == VER_630) {
	    PerfMon::Select(1, 19); /* TLB Misses */
	    PerfMon::Select(2, 0);  /* Instructions Completed */
	}
	break;
    }
    return;
}

void
PerfMon::Measure(Events choice)
{
    if (KernelInfo::OnSim())
	return;

    PerfMon::measure = choice;

    return;
}

void
PerfMon::Print()
{
    uval32 pmc1, pmc2, pmc3, pmc4, pmc5, pmc6, pmc7, pmc8;

    if (KernelInfo::OnSim())
	return;

    if (PerfMon::measure == PerfMon::NONE)
	return;

    VPNum vp = Scheduler::GetVP();

    if (vp == 0)
	switch (PerfMon::measure) {
	default:
	    break;
	case BUS:
	    if (_BootInfo->cpu_version == VER_APACHE) {
		err_printf("pm    VP   ABus Total  ABus Local  DBus Total  DBus Local\n");
	    } else if (_BootInfo->cpu_version == VER_630) {
		err_printf("pm    VP\n");
	    }
	    break;
	case DCACHE:
	    if (_BootInfo->cpu_version == VER_APACHE) {
		err_printf("pm    VP   DCache Miss  Stor Delay  DCache Stall  DCache Invalidates\n");
	    } else if (_BootInfo->cpu_version == VER_630) {
		err_printf("pm    VP     Cycles   Insns Compl  L1 Load Miss  L2 M->E or S\n");
		err_printf("pm    VP     L2 M->I  DCache Invalidates\n");
	    }
	    break;
	case INSN:
	    if (_BootInfo->cpu_version == VER_APACHE) {
		err_printf("pm    VP   Sync Stalls  FXU Insns  Cycles  Insns Executed\n");
	    } else if (_BootInfo->cpu_version == VER_630) {
		err_printf("pm    VP     Cycles   Insns Compl  Sync Reruns\n");
	    }
	    break;
	case TLB:
	    if (_BootInfo->cpu_version == VER_APACHE) {
		err_printf("pm    VP     DTLB Miss  TLBIE All  TLBIE Local  DTLB TLBIE Hits\n");
	    } else if (_BootInfo->cpu_version == VER_630) {
		err_printf("pm    VP\n");
	    }
	    break;
	}

    if (_BootInfo->cpu_version == VER_APACHE) {
	PerfMon::GetPMC(1, &pmc1);
	PerfMon::GetPMC(2, &pmc2);
	PerfMon::GetPMC(3, &pmc3);
	PerfMon::GetPMC(4, &pmc4);
    } else if (_BootInfo->cpu_version == VER_630) {
	PerfMon::GetPMC(1, &pmc1);
	PerfMon::GetPMC(2, &pmc2);
	PerfMon::GetPMC(3, &pmc3);
	PerfMon::GetPMC(4, &pmc4);
	PerfMon::GetPMC(5, &pmc5);
	PerfMon::GetPMC(6, &pmc6);
	PerfMon::GetPMC(7, &pmc7);
	PerfMon::GetPMC(8, &pmc8);
    }

    if (_BootInfo->cpu_version == VER_APACHE) {
	err_printf("pm %5ld  %11d %11d %11d %11d\n",
		   vp, pmc1, pmc2, pmc3, pmc4);
    } else if (_BootInfo->cpu_version == VER_630) {
	err_printf("pm %4ld %10d %10d %10d %10d\n",
		   vp, pmc1, pmc2, pmc3, pmc4);
	err_printf("pm %4ld %10d %10d %10d %10d\n",
		   vp, pmc5, pmc6, pmc7, pmc8);
    }

    return;
}

void
PerfMon::Zero()
{
    if (KernelInfo::OnSim())
	return;

    if (_BootInfo->cpu_version == VER_APACHE) {
	PerfMon::SetPMC(1, 0);
	PerfMon::SetPMC(2, 0);
	PerfMon::SetPMC(3, 0);
	PerfMon::SetPMC(4, 0);
    } else if (_BootInfo->cpu_version == VER_630) {
	PerfMon::SetPMC(1, 0);
	PerfMon::SetPMC(2, 0);
	PerfMon::SetPMC(3, 0);
	PerfMon::SetPMC(4, 0);
	PerfMon::SetPMC(5, 0);
	PerfMon::SetPMC(6, 0);
	PerfMon::SetPMC(7, 0);
	PerfMon::SetPMC(8, 0);
    }
    return;
}
