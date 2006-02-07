/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BootInit.C,v 1.3 2005/04/29 01:00:12 mergen Exp $
 *****************************************************************************/
//#define __KERNEL__
//#include "lkKernIncs.H"
#include "kernIncs.H"
#include "bilge/arch/powerpc/BootInfo.H"
extern "C" {
#include <linux/types.h>
#include <asm/lmb.h>
}

#include <init/MemoryMgrPrimitiveKern.H>
#include <init/BootPrintf.H>
extern struct lmb lmb;

extern "C" unsigned long num_physpages;
unsigned long num_physpages;

void
relocateLMB(MemoryMgrPrimitiveKern *memory)
{
    for (uval i = 0; i < MAX_LMB_REGIONS + 1; ++i) {
	if (lmb.memory.region[i].size != 0) {
	    lmb.memory.region[i].base += memory->virtBase();
	}
	if (lmb.reserved.region[i].size != 0) {
	    lmb.reserved.region[i].base += memory->virtBase();
	}
    }
}


static void
advance(uval &curr, uval next, char *label, MemoryMgrPrimitiveKern *memory)
{
    passertMsg(curr <= next, "Inconsistency in LMB structures.\n");
    if (curr < next) {
	BootPrintf::Printf("%s: 0x%016lx (0x%016lx bytes)\n",
		   label, curr, next - curr);
	if (memory != NULL) {
	    uval virtBase = memory->virtBase();
	    memory->rememberChunk(virtBase + curr, virtBase + next);
	}
	curr = next;
    }
}

void
scanLMBForMem(MemoryMgrPrimitiveKern *memory)
{
    uval curr;
    passertMsg(sizeof(lmb)==sizeof(_BootInfo->lmb),
	       "Mismatch between linux and k42 lmb structures");

    memcpy((void*)&lmb, (void*)&_BootInfo->lmb, sizeof(lmb));


    curr = 0;
    uval total = 0;
    for (uval i = 0; i < lmb.memory.cnt; i++) {
	lmb_property *mem = &lmb.memory.region[i];
	advance(curr, mem->base, "Mem hole ", NULL);
	for (uval j = 0; j < lmb.reserved.cnt; j++) {
	    lmb_property *res = &lmb.reserved.region[j];
	    if (((res->base + res->size) > mem->base) &&
		(res->base < (mem->base + mem->size)))
		{

		    total -= res->size;
		    advance(curr, res->base, "Available", memory);
		    advance(curr, res->base + res->size, "Reserved ", NULL);
		}
	}

      if (_BootInfo->onHV) {
// FIXME MFM kludge til we detect RMO properly, use non-RMO appropriately
	total += 0x8000000ULL;
      } else {
	total += mem->size;
	advance(curr, mem->base + mem->size, "Available", memory);
      }
    }

    relocateLMB(memory);
    num_physpages = total / PAGE_SIZE;
}
