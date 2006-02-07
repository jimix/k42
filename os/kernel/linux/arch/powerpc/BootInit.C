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

#if 0  /* Linux already filled out lmb for us.  */
    memcpy((void*)&lmb, (void*)&_BootInfo->lmb, sizeof(lmb));
#endif

    struct lmb * l = &lmb;

#if 0
l->debug = 0x0;
l->rmo_size = 0x8000000;

l->memory.cnt = 0x1;
l->memory.size = 0x8000000;

l->memory.region[0].base = 0x0;
l->memory.region[0].physbase = 0x0;
l->memory.region[0].size = 0x8000000;

l->reserved.cnt = 0x3;
l->reserved.size = 0x0;

l->reserved.region[0].base = 0x0;
l->reserved.region[0].physbase = 0x0;
l->reserved.region[0].size = 0x3000;

l->reserved.region[1].base = 0x1fe0000;
l->reserved.region[1].physbase = 0x1fe0000;
l->reserved.region[1].size = 0x3020000;

l->reserved.region[2].base = 0x5f00000;
l->reserved.region[2].physbase = 0x5f00000;
l->reserved.region[2].size = 0x100000;

l->reserved.region[3].base = 0x5f00000;
l->reserved.region[3].physbase = 0x5f00000;
l->reserved.region[3].size = 0x100000;
#endif

    curr = 0;
    uval total = 0;
    for (uval i = 0; i < l->memory.cnt; i++) {
	lmb_property *mem = &lmb.memory.region[i];
	advance(curr, mem->base, "Mem hole ", NULL);
	for (uval j = 0; j < lmb.reserved.cnt; j++) {
	    lmb_property *res = &lmb.reserved.region[j];
	    if (((res->base + res->size) > mem->base) &&
		(res->base < (mem->base + mem->size)))
		{
#if 1
		    uval s = PAGE_ROUND_UP(res->size);
                    uval b = PAGE_ROUND_DOWN(res->base);
		    uval n = b + s;

		    if (b < curr) b = curr;

		    total -= s;
		    advance(curr, b, "Available", memory);
		    advance(curr, n, "Reserved ", NULL);
#else
		    total -= res->size;
		    advance(curr, res->base, "Available", memory);
		    advance(curr, res->base + res->size, "Reserved ", NULL);
#endif
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
