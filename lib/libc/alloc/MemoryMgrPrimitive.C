/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryMgrPrimitive.C,v 1.11 2005/03/15 02:37:53 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Primitive space allocator - used only during processor initialization.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "MemoryMgrPrimitive.H"

void
MemoryMgrPrimitive::checkAlign(char *name, uval value)
{
    // no printf, since might not be safe early enough that
    // this is used
    if (PAGE_ROUND_DOWN(value) != value) breakpoint();
}

void
MemoryMgrPrimitive::recoverSpace(uval start, uval end)
{
    if ((PAGE_ROUND_UP(start) + PAGE_SIZE) < PAGE_ROUND_DOWN(end)) {
	rememberChunk(PAGE_ROUND_UP(start), PAGE_ROUND_DOWN(end));
    }
}

void
MemoryMgrPrimitive::init(uval allocstart, uval allocend)
{
    init(allocstart, allocend, sizeof(uval));
}

void
MemoryMgrPrimitive::init(uval allocstart, uval allocend, uval minalign)
{
    checkAlign("allocStart", allocstart);
    checkAlign("allocEnd",   allocend);

    _allocStart = allocstart;
    _allocEnd   = allocend;
    _allocNext  = _allocStart;

    _numSavedChunks = 0;

    minAlign = minalign;
}

void
MemoryMgrPrimitive::rememberChunk(uval start, uval end)
{
    checkAlign("start", start);
    checkAlign("end",   end);

    if (start > end) breakpoint();
    if (_numSavedChunks >= MAX_SAVED_CHUNKS) breakpoint();

    _savedChunk[_numSavedChunks].start = start;
    _savedChunk[_numSavedChunks].end = end;
    _numSavedChunks++;
}

uval
MemoryMgrPrimitive::retrieveChunk(uval &start, uval &end)
{
    while (_numSavedChunks != 0) {
	_numSavedChunks--;
	start = _savedChunk[_numSavedChunks].start;
	end = _savedChunk[_numSavedChunks].end;
	// the code which allocates from the saved chunk array sometimes
	// leaves a zero length chunk - treat it here as not existing
	if (end > start) return 1;
    }
    return 0;
}

void
MemoryMgrPrimitive::alloc(uval &ptr, uval size, uval align, uval offset)
{
    align = align < minAlign ? minAlign : align;
    if (offset >= align) breakpoint();
    ptr = ((_allocNext + (align - offset - 1)) & ~(align - 1)) + offset;
    size = ALIGN_UP(size, minAlign);
    if (ptr + size > _allocEnd) breakpoint();
    recoverSpace(_allocNext, ptr);
    _allocNext = ptr + size;

#ifndef NDEBUG
    // Trash up to a page of each allocation.  No one should assume
    // zero-filled memory from the primitive allocator.
    memset((void *) ptr, 0xbf, MIN(size, PAGE_SIZE));
#endif /* #ifndef NDEBUG */
}

/*
 * N.B. storage allocated from chunks is NOT addressable early.
 *
 */
SysStatus
MemoryMgrPrimitive::allocFromChunks(
    uval &ptr, uval size, uval align, uval offset)
{
    align = align < minAlign ? minAlign : align;
    if (offset >= align) return _SERROR(2875, 0, ENOMEM);
    uval n = _numSavedChunks;
    uval start, end;
    while (n > 0) {
	n--;
	start = _savedChunk[n].start;
	end = _savedChunk[n].end;
	ptr = ((start + (align - offset - 1)) & ~(align - 1)) + offset;
	if (ptr + size > end) continue;
	if (ptr > start) {
	    _savedChunk[n].end = ptr;
	    // ptr+start may equal end - see recoverSpace
	    recoverSpace(ptr+start, end);
	} else {
	    // may equal end - see retrieveChunk
	    _savedChunk[n].start = ptr+size;
	}
	// do not try to dirty this memory for debug - it may not
	// yet be addressable.  The expected use of this path is
	// allocation of a large page table
	return 0;
    }
    return _SERROR(2875, 0, ENOMEM);
}

/*
 * align allocNext to next page boundary and return its new value
 */
uval
MemoryMgrPrimitive::allocNextPage()
{
    _allocNext = ALIGN_UP(_allocNext, PAGE_SIZE);
    return _allocNext;
}

void
MemoryMgrPrimitive::allocAll(uval &ptr, uval &size, uval align, uval offset)
{
    if (offset >= align) breakpoint();
    ptr = ((_allocNext + (align - offset - 1)) & ~(align - 1)) + offset;
    recoverSpace(_allocNext, ptr);
    size = (_allocEnd - ptr) & ~(align - 1);
    recoverSpace(ptr + size, _allocEnd);
    _allocNext = _allocEnd;
}

uval
MemoryMgrPrimitive::memorySize()
{
    uval size;
    uval chunk;

    size = 0;
    for (chunk = 0; chunk < _numSavedChunks; chunk++)
    {
	size += _savedChunk[chunk].end -
    		_savedChunk[chunk].start;
    }

    return size;
}

#include <misc/hardware.H>
void
MemoryMgrPrimitive::touchAllocated()
{
    uval vs = PAGE_ROUND_DOWN(_allocStart);
    uval ve = _allocEnd;
    for (;vs<ve;vs+=PAGE_SIZE) {
	*(volatile uval*)vs;
    }
}
