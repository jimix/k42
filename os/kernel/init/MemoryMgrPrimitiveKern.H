#ifndef __MEMORY_MGR_PRIMITIVE_KERN_H_
#define __MEMORY_MGR_PRIMITIVE_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryMgrPrimitiveKern.H,v 1.9 2005/04/15 17:39:36 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Primitive space allocator - used only during processor initialization.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>

class MemoryMgrPrimitiveKern : public MemoryMgrPrimitive {
    uval _physStart;	// start of physical memory
    uval _physEnd;	// end of physical memory
    uval _virtBase;	// virtual address corresponding to physical address 0

public:
    void init(uval aPhysStart, uval aPhysEnd, uval aVirtBase,
	      uval allocstart, uval allocend)
	{
	    init(aPhysStart, aPhysEnd, aVirtBase, allocstart, allocend,
		 sizeof(uval));
	}

    void init(uval aPhysStart, uval aPhysEnd, uval aVirtBase,
	      uval allocstart, uval allocend, uval mAlign)
    {
	MemoryMgrPrimitive::init(allocstart, allocend, mAlign);

	checkAlign("physStart",  aPhysStart);
	checkAlign("physEnd",    aPhysEnd);
	checkAlign("virtBase",   aVirtBase);

	_physStart  = aPhysStart;
	_physEnd    = aPhysEnd;
	_virtBase   = aVirtBase;
    }

    uval physFromVirt(uval virt) { return (virt - _virtBase); }
    uval virtFromPhys(uval real) { return (_virtBase + real); }
    uval physStart()		 { return _physStart; }
    uval physEnd()		 { return _physEnd; }
    uval virtBase()		 { return _virtBase; }
};

#endif /* #ifndef __MEMORY_MGR_PRIMITIVE_KERN_H_ */
