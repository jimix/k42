#ifndef __FREEFRAMELISTCHUNK_H_
#define __FREEFRAMELISTCHUNK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeFrameListChunk.H,v 1.3 2005/06/27 06:15:52 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains lists of free page frames, used in
 * PM classes and soon FCMs, all locking outside of these classes.
 * **************************************************************************/

#include "FreeFrameList.H"

class FreeFrameListChunk {
public:
    static const uval PER_FRAME_ELEM=509;
protected:
    friend class FreeFrameListOfLists;
    struct FrameInfo {
	FrameInfo *next;
	FrameInfo *prev;
	uval frameCount;
	uval pages[PER_FRAME_ELEM];
    };
    FrameInfo *tail;
    FrameInfo *head;
    uval count;
    uval chunkSize;
    void kosher();

private:
    FreeFrameList looseFrames;

public:
    void init() { head=NULL; tail=NULL; count=0; }
    FreeFrameListChunk(uval size) 
	: chunkSize(size) {
	init(); 
    }

    uval getCount() { return count + looseFrames.getCount(); }
    uval isNotEmpty() { return count + looseFrames.getCount(); }

    // Will keep track of these chunks
    void freeChunkOfFrames(uval addr, uval size) {
	FrameInfo *fi;
	uval numFrames = size/PAGE_SIZE;

	fi = (FrameInfo *)addr;
	if (head != NULL) {
	    fi->next = head;
	    head->prev = fi;
	} else {
	    fi->next = NULL;
	    tail = fi;
	}
	fi->prev = NULL;
	fi->frameCount = numFrames-1; // One lost as overhead
	count += fi->frameCount;
	head = fi;

	addr += PAGE_SIZE;
	for (uval i=0; i<fi->frameCount; i++, addr+=PAGE_SIZE) {
	    head->pages[i] = addr;
	}
	kosher();
    }


    uval getChunkOfFrames(uval *addr, uval *size) {
	uval oldCount = count;
	if (head==NULL) {
	    if (looseFrames.getCount()) {
		*addr = looseFrames.getFrame();
		*size = PAGE_SIZE;
		return 1;
	    } else {
		return 0;
	    }
	}

	if (head->frameCount==(chunkSize/PAGE_SIZE-1)) {
	    // All of the frames in the chunk have been freed
	    *addr = uval(head);
	    *size = chunkSize;
	    head = head->next;
	    count -= (chunkSize/PAGE_SIZE)-1;
	} else if (head->frameCount== 0) {
	    *addr = uval(head);
	    *size = PAGE_SIZE;
	    head = head->next;
	    // We don't decrement the count when the head
	    // frame is removed as it wasn't counted in the first place
	    // as being available
	} else {
	    head->frameCount--;
	    count--;
	    *addr = head->pages[head->frameCount];
	    *size = PAGE_SIZE;
	}

	passertMsg(oldCount >= count, "frame frame list chunk count bad\n");

	kosher();
	return 1;
    }

    void freeFrame(uval addr) {
	// Look for appropriate bin to put free frame in
	FrameInfo *bin = head;

	while (bin) {
	    if ( (addr > uval(bin)) && (addr < uval(bin) + chunkSize) ) {
		// We have the right bin
		break;
	    }
	    bin = bin->next;
	}

	if (bin==NULL) {
	    // We didn't allocate this frame from a chunk
	    // probably left over from a fork
	    looseFrames.freeFrame(addr);
	} else {
	    bin->pages[bin->frameCount] = addr;
	    bin->frameCount++;
	    count++;
	}

	kosher();
    }
	    
    // 0 means no frame available
    uval getFrame() {
	FrameInfo *bin = head;
	uval addr;
	kosher();

	if (count == 0) {
	    // Use a loose frame as a last resort
	    return looseFrames.getFrame();
	}
	count--;

	while (bin && bin->frameCount==0) {
	    bin = bin->next;
	}

	passertMsg(bin!=NULL, "Could not find bin with free page\n");

	bin->frameCount--;
	addr = bin->pages[bin->frameCount];
	tassertMsg(addr, "bad address returned\n");
	tassertMsg((((uval)addr) & PAGE_MASK) == 0, 
		   "%lx not page aligned?\n", (uval)addr);
	kosher();
	return addr;
    }
	
    void freeList(FreeFrameList *ffl);
};
    
#endif /* __FREEFRAMELISTCHUNK_H_ */
