/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Buffer.C,v 1.12 2005/08/29 14:13:55 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "Buffer.H"
#include <stub/StubRegionDefault.H>
#include <stub/StubFRPA.H>
#include <mem/Access.H>


#include <defines/paging.H>

Buffer::Buffer(uval flen, uval initOff, uval flags, ObjectHandle oh,
	       uval ut, uval currentOpLen) :
    stubFr(StubObj::UNINITIALIZED),
    stub(StubObj::UNINITIALIZED),
    isFlengthOffsetKnown(1), flength(flen), initialOffset(initOff)
{
    SysStatus rc = this->init(flags, oh, ut, currentOpLen);
    tassertMsg(_SUCCESS(rc), "?");
}

Buffer::Buffer(uval flags, ObjectHandle oh, uval ut, uval currentOpLen) :
    stubFr(StubObj::UNINITIALIZED),
    stub(StubObj::UNINITIALIZED),
    isFlengthOffsetKnown(0)
{
    SysStatus rc = this->init(flags, oh, ut, currentOpLen);
    tassertMsg(_SUCCESS(rc), "?");
}

SysStatus
Buffer::init(uval flags, ObjectHandle oh, uval ut, uval currentOpLen)
{
    openFlags = flags;
    useType = ut;

    bufferType = NONE;
    smallBuf = NULL;
    smallOffset = uval(~0); /* trying to easily detect if things are not
			     * properly setup */
    smallBufSize = 0;
    smallBufModified = 0;
    smallBufModStart = 0;
    // initialize stubFr as invalid
    ObjectHandle tmpoh;
    tmpoh.init();
    stubFr.setOH(tmpoh);

    stub.setOH(oh);

    SysStatus rc;
    if (currentOpLen < FileLinux::MAX_SMALLFILE_SIZE) {
	rc = bufferSetup();
    } else {
	rc = initCbuf();
    }
    tassertMsg(_SUCCESS(rc), "ops");
    tassertMsg(bufferType == MMAP || bufferType == SMALL_BUFFER,
	       "bufferType %ld\n", bufferType);
    tassertMsg(isFlengthOffsetKnown == 0 || flength >= 0, "?");
    return 0;
}

/* virtual */ SysStatus
Buffer::getLengthOffset(uval &flen, uval &off)
{
    tassertMsg(bufferType == MMAP || bufferType == SMALL_BUFFER,
	       "bufferType %ld\n", bufferType);
    if (bufferType == MMAP) {
	if (useType != FileLinux::NON_SHARED || isFlengthOffsetKnown == 0) {
	    SysStatus rc = stub._getLengthOffset(flen, off, 0, 0);
	    tassertMsg(_SUCCESS(rc), "?");
	    isFlengthOffsetKnown = 1;
	    cbuf.setOff(off);
	    flength = flen;
	    if (flength > off) {
		cbuf.setCnt(flength-off);
	    } else {
		cbuf.setCnt(0);
	    }
	} else {
	    tassertMsg(isFlengthOffsetKnown == 1, "?");
	    flen = flength;
	    off = cbuf.getOff();
	}
    } else {
	tassertMsg(isFlengthOffsetKnown == 1, "?");
	flen = flength;
	off = smallOffset;
    }

    return 0;
}

SysStatus
Buffer::initCbuf()
{
    SysStatus rc;

    bufferType = MMAP;

    if (stubFr.getOH().invalid()) {
	ObjectHandle FRoh;
	rc = stub._getFROH(FRoh);
	tassertMsg(_SUCCESS(rc), "getFROH failed rc is (%ld, %ld, %ld)\n",
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	stubFr.setOH(FRoh);
    }

    cbuf.setRefs(0);

    // FIXME - define MAX_REGION_SIZE per architecture
#if defined(TARGET_powerpc)
    // Make the size be an odd number of segments so that consecutive
    // allocations use different segment table buckets.
    uval size = (1ULL<<40) + (1ULL<<28);
#else
    uval size = 1ULL<<30;
#endif

    if ((O_ACCMODE & openFlags) == O_RDONLY) {
	if (isFlengthOffsetKnown) {
	    // got flength from server on registration of call back
	    // FIXME dilma: we shouldn't even map a read only file with
	    // size 0
	    size = (flength == 0 ? 1 : flength);
	    flengthFR = flength;
	}
    }

    rc = attachNewCbuf(0, size, 1);
    tassertMsg(_SUCCESS(rc), "ops\n");
    if (isFlengthOffsetKnown) {
	if (initialOffset != 0) {
	    cbuf.setOff(initialOffset);
	    if (flength > initialOffset) {
		cbuf.setCnt(flength - initialOffset);
	    } else {
		cbuf.setCnt(0);
	    }
	}
    } else {
	cbuf.setOff(0);
	cbuf.setCnt(0);
    }

    return rc;
}

SysStatus
Buffer::attachNewCbuf(uval offset, uval allocSize, uval noDetachPrev)
{
    // will attach a buffer that is maximum size possible
    if (!noDetachPrev) {
	// detach previous Cbuf
	if (cbuf.getRefs()) {
	    IOBufMap *bf = createIOBufMap(&cbuf);
	    bufList.append(bf);
	    cbuf.setRefs(0);
	}
    }

    // attach a new buffer

    // FIXME: don't yet support offset
    tassertMsg((offset==0), "IOMapped::attachNewCbuf offset NYI\n");

    cbuf.setOff(offset);
    cbuf.setFileOffset(0);

    // bind in the new region
    uval regadd, reglength;
    reglength = allocSize;
    cbuf.setLen(reglength);

    SysStatus rc = StubRegionDefault::_CreateFixedLenExt(
	regadd, reglength, 0/*alignmentreq*/, stubFr.getOH(), 0/*fileOffset*/,
	AccessMode::writeUserWriteSup, 0/*target*/,
	RegionType::K42Region);
    tassertMsg(_SUCCESS(rc), "bindregion failed\n");

    cbuf.setCnt(flength-offset);
    cbuf.setBase((char *)regadd);
    return 0;
}

/* virtual */ SysStatus
Buffer::readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    if (bufferType == SMALL_BUFFER) {
	SysStatusUval rclen = readSmallBuffer(len, buf, tw);
	return rclen;
    }

    // check cbuf and allocate from it if available
    // else check file length if changed
    //   if it has changed, allocate a larger region...
    // for now minimum imp just return whatever have

    tassertMsg((useType != FileLinux::NON_SHARED
		|| flength==cbuf.getCnt()+cbuf.getOff()),
	       "obj %p, flength %ld, getCnt()+cbuf.getOff() %ld\n", this,
	       flength, cbuf.getCnt()+cbuf.getOff());

    if (useType != FileLinux::NON_SHARED) {
	// retrieve file length and offset for server
	// update cbuf.setCnt(), cbuf.setOff(), flength
	uval offset;
	SysStatus rc = stub._getLengthOffset(flength, offset, 0, len);
	tassertMsg(_SUCCESS(rc), "_getLengthOffsetFromFS failed\n");
	isFlengthOffsetKnown = 1;
	cbuf.setOff(offset);
	if (flength > offset) {
	    cbuf.setCnt(flength-offset);
	} else {
	    cbuf.setCnt(0);
	}
    } else {
	// it has to be initialized at construction
	tassertMsg(isFlengthOffsetKnown == 1, "?");
    }

    len = MIN(len, cbuf.getCnt());

    if (len > 0) {
	tassertMsg((cbuf.getOff()+len <= cbuf.getLen()),
		   "reading (off 0x%lx, len 0x%lx) beyond allocated memory"
		   "(0x%lx)\n", cbuf.getOff(), len, cbuf.getLen());

	buf = cbuf.getPtr();
	cbuf.decCnt(len);
	cbuf.incPtr(len);
	cbuf.incRefs();
	return len;
    }
    buf = 0;

    return _SERROR(1335, FileLinux::EndOfFile, 0);
}

/* virtual */ SysStatus
Buffer::readFree(char *ptr)
{
    if (bufferType == MMAP) {
	cbuf.decRefs();
    } else {
	tassertMsg(bufferType == SMALL_BUFFER, "bufferType is %ld\n",
		   bufferType);
	smallBufRefs--;
    }
    return 0;
}

/* virtual */ SysStatusUval
Buffer::writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    if (bufferType == SMALL_BUFFER) {
	if (flength + len > smallBufSize) {
	    //err_printf("buffer needs to become larger\n");
	    smallBufToMmap();
	} else {
	    return writeSmallBuffer(len, buf, tw);
	}
    }

    tassertMsg(bufferType == MMAP, "bufferType %ld\n", bufferType);
    tassertMsg(cbuf.getRefs() >= 0, "?");

    // flength is length the file system "thinks" this file has
    // the length we think the file has may be bigger
    // it will be the upper bound of the ends of all the Alloc's
    // cbuf.getLen is the size of the region available to grow the
    //  file in.  For now, we don't know how to enlarge this!

    // We assume that our cbuf's all start at the beginning of
    // the file.

    tassertMsg((useType != FileLinux::NON_SHARED
		|| flength == cbuf.getCnt()+cbuf.getOff()
		|| flength < cbuf.getOff()),
	       "flength %ld, getCnt %ld, cbuf.getOff %ld\n",
	       flength, cbuf.getCnt(), cbuf.getOff());

    if (useType != FileLinux::NON_SHARED) {
	// retrieve file length and offset for server
	uval offset;
	SysStatus rc = stub._getLengthOffset(flength, offset, 1, len);
	tassertMsg(_SUCCESS(rc), "_getLengthOffset failed\n");
	isFlengthOffsetKnown = 1;
	if (openFlags & O_APPEND) {
	    offset = flength;
	}
	cbuf.setOff(offset);
	if (flength > offset) {
	    cbuf.setCnt(flength-offset);
	} else {
	    cbuf.setCnt(0);
	}
    } else {
	tassertMsg(isFlengthOffsetKnown == 1, "?");
	if (openFlags & O_APPEND) {
	    cbuf.setOff(flength);
	    cbuf.setCnt(0);
	}
    }

    tassertMsg((cbuf.getOff()+len <= cbuf.getLen()),
	       "writing (at 0x%lx) beyond allocated memory(0x%lx)\n",
	       cbuf.getOff()+len-1, cbuf.getLen());

    /* first grow "our" file length if necessary to fit this write.
     * (For example, in an append sequence, cnt will always be 0!)
     */
    if (cbuf.getCnt() < len)
	cbuf.setCnt(len);

    if (len > 0) {
	buf = cbuf.getPtr();
	cbuf.decCnt(len);
	cbuf.incPtr(len);
	cbuf.incRefs();
	return len;
    }
    buf = 0;

    return 0;
}

/* virtual */ SysStatus
Buffer::writeFree(char *ptr)
{
    if (bufferType == MMAP) {
	tassertMsg(cbuf.getRefs() > 0, "?");
	// FIXME: not yet checking list
	// whenever there are no writes outstanding our length is "correct"
	// and we can update the file representative information.
	// we only do it when change is "big" enough.
	if (cbuf.decRefs() == 0) {
	    if (useType == FileLinux::NON_SHARED) {
		uval oldflength = flength;
		flength = cbuf.getOff()+cbuf.getCnt();
		if (PAGE_ROUND_DOWN(oldflength) != PAGE_ROUND_DOWN(flength)) {
		    SysStatus rc = stub._ftruncate(flength);
		    tassertMsg(_SUCCESS(rc), "failure\n");
		    flengthFR = flength;
		}
	    } else {
		/* FIXME: this scheme to keep the file length is completely
		 * bogus; two concurrent executions of pairs
		 * locked_writeAlloc-locked_writeFree could be intertwined
		 * to result in a wrong final size (bigger or smaller).
		 * For now this is not biting us, and the problem will be
		 * fixed once we migrate to a correct protocol for Unix
		 * semantics for read/write */
		flength=cbuf.getOff()+cbuf.getCnt();
		SysStatus rc = stub._ftruncate(flength);
		tassertMsg(_SUCCESS(rc), "failure\n");
		flengthFR = flength;
	    }
	} else {
	    // tassertMsg(0, "In the case where cbuf.decRefs() was not 0\n");
	}
    } else {
	tassertMsg(bufferType == SMALL_BUFFER, "bufferType is %ld\n",
		   bufferType);
	smallBufRefs--;
    }

    return 0;
}

/* virtual */ SysStatus
Buffer::flush(uval release)
{
    if (bufferType == MMAP) {
	stubFr._fsync();
    } else if (bufferType == SMALL_BUFFER) {
	uval modStart = 0;
	uval modLen = 0;

	if ((O_ACCMODE & openFlags) != O_RDONLY  &&
	    flength > 0 && smallBufModified!=0) {
	    modStart = smallBufModStart;
	    modLen = smallBufModified;
	}
	tassertMsg(flength >= modStart+modLen,"Modified range out of file\n");
	SysStatus rc = stub._syncSharedBuf(flength, modStart, modLen, release);
	passertMsg(_SUCCESS(rc), "ops\n");
	if (release) {
	    smallBufModified = 0;
	    smallBufModStart = 0;
	    smallBuf = NULL;
	}
    } else {
	tassertMsg(bufferType == NONE, "bufferType %ld\n", bufferType);
    }
    return 0;
}

/* virtual */ SysStatusUval
Buffer::setFilePosition(sval position, FileLinux::At at)
{
    tassertMsg(bufferType == MMAP || bufferType == SMALL_BUFFER,
	       "bufferType %ld\n", bufferType);

    if (useType == FileLinux::NON_SHARED) {
	switch (at) {
	case FileLinux::ABSOLUTE:
	    break;
	case FileLinux::RELATIVE:
	    if (bufferType == MMAP) {
		position += cbuf.getOff();
	    } else {
		position += smallOffset;
	    }
	    break;
	case FileLinux::APPEND:
	    position += flength;
	    break;
	default:
	    return _SERROR(1843, 0, EINVAL);
	}

	if (position < 0) {
	    return _SERROR(1844, 0, EINVAL);
	}

	if (bufferType == MMAP) {
	    // position is guaranteed to be >= 0
	    cbuf.setOff(position);
	    if (flength > (uval)position) {
		cbuf.setCnt(flength-position);
	    } else {
		flength = position;
		cbuf.setCnt(0);
	    }
	} else {
	    smallOffset = position;
	    if (flength < (uval) position) {
		flength = (uval) position;
	    }
	}

	return position;
    } else {
	// go through stub
	return stub._setFilePosition(position, at);
    }
}

/* virtual */ SysStatus
Buffer::ftruncate(uval length)
{
    SysStatus rc;

    // FIXME: no small buffer support here???
    tassertMsg(bufferType == MMAP, "bufferType %ld\n", bufferType);

    rc = stub._ftruncate((uval) length);
    _IF_FAILURE_RET(rc);

    flength = flengthFR = length;
    if (useType == FileLinux::NON_SHARED && bufferType == MMAP) {
	if (flength > cbuf.getOff()) {
	    cbuf.setCnt(flength - cbuf.getOff());
	} else {
	    cbuf.setCnt(0);
	}
    }
    return rc;
}

Buffer::~Buffer()
{
    SysStatus rc;

    switch (bufferType) {
    case MMAP:
	tassertMsg(cbuf.getRefs() == 0, "buffer destructor with "
		   "getRefs()=%ld\n", cbuf.getRefs());

	rc = DREFGOBJ(TheProcessRef)->regionDestroy((uval)cbuf.getBase());
	// FIXME: return this rc?
	tassertMsg(_SUCCESS(rc), "woops\n");

#ifndef ENABLE_SYNCSERVICE
	flush();
#endif // #ifndef ENABLE_SYNCSERVICE

	rc = stubFr._releaseAccess();
	tassertMsg(_SUCCESS(rc), "FR releaseAccess failed\n");

	break;

    case SMALL_BUFFER:

	tassertMsg(smallBufRefs == 0, "FileLinuxFile::Destroy with "
		   "smallBufRefs %ld\n", smallBufRefs);
	flush(1);
	smallBuf = NULL;

	break;

    case NONE:
	passertMsg(stubFr.getOH().valid() == 0,
		   "report this to Dilma please\n");
	break;
    default:
	tassertMsg(0, "bufferType is %ld\n", bufferType);
    }
}

/* virtual */ SysStatus
Buffer::switchToShared(uval &flen, uval &off)
{
    flen = flength;
    if (bufferType == SMALL_BUFFER) {
	off = smallOffset;
	smallBufToMmap();
    } else {
	tassertMsg(bufferType == MMAP, "bufferType %ld\n", bufferType);
	off = cbuf.getOff();
	if (flengthFR != flength) {
	    stub._ftruncate(flength);
	    flengthFR = flength;
	}
    }

    useType = FileLinux::SHARED;

    tassertMsg(bufferType == MMAP, "bufferType is %ld\n", bufferType);

    return 0;
}

SysStatus
Buffer::bufferSetup()
{
    tassertMsg(bufferType == NONE, "bufferType is %ld\n", bufferType);
    tassertMsg((useType == FileLinux::NON_SHARED
		|| useType == FileLinux::SHARED
		|| useType == FileLinux::FIXED_SHARED),
	       "invalid useType %ld\n",
	       (uval) useType);

    tassertMsg(flength >= 0, "?");

    if (KernelInfo::ControlFlagIsSet(KernelInfo::SMALL_FILE_OPT)) {
	// small file optimization is on
	SysStatus rc = 0;
	if (useType == FileLinux::NON_SHARED
	    && flength < FileLinux::MAX_SMALLFILE_SIZE) {
	    //err_printf("small file with flength %ld\n", flength);
	    rc = initSmallBuf();
	    if (_SUCCESS(rc)) {
		return rc;
	    } else {
		/* either shared or (non shared but large) or (non shared,
		 * small, but the init of small buffer failed */
		if (_FAILURE(rc)) {
		    /* If the file system doesn't support the _read
		     * interface,  we don't need to look at the problem.
		     * Otherwise, let's see  what's going on */
		    //tassertWrn(0, "init for small buf failed");
		    tassertMsg(_SGENCD(rc) == ENOENT, "?");
		}
	    }
	}
    }
    return initCbuf();
}

SysStatus
Buffer::initSmallBuf()
{
    SysStatus rc;
    tassertMsg(isFlengthOffsetKnown == 1, "?");

    passertMsg(flength <= FileLinux::MAX_SMALLFILE_SIZE,
	       "flength %ld\n", flength);

    uval shOffset;
    ObjectHandle oh;
    smallBufRefs = 0;
    smallBufSize = FileLinux::MAX_SMALLFILE_SIZE;
    smallBufModStart = 0;
    smallBufModified = 0;

    rc = stub._getSharedBuf(oh, shOffset, smallBufSize);
    _IF_FAILURE_RET(rc);

    rc = ShMemClnt::Get(oh, shmemRef);

    _IF_FAILURE_RET(rc);

    uval x;
    rc = DREF(shmemRef)->offsetToAddr(shOffset, x);
    smallBuf = (char*)x;
    _IF_FAILURE_RET(rc);

    passertMsg(smallBuf != NULL, "out of space\n");

    passertMsg(initialOffset <= smallBufSize && initialOffset >= 0,
	       "initialOffset %ld, smallBufSize %ld\n", initialOffset,
	       smallBufSize);
    smallOffset = initialOffset;

    flengthFR = 0;
    bufferType = SMALL_BUFFER;
    return 0;
}

SysStatus
Buffer::smallBufToMmap()
{
    tassertMsg(smallBuf != NULL, "wrong\n");

    SysStatus rc;

    // FIXME: this is a first hack; looking at it properly we can save
    // some PPC calls!
    rc = stub._ftruncate(0);
    tassertMsg(_SUCCESS(rc), "failure\n");
    initialOffset = 0;
    rc = initCbuf();
    tassertMsg(_SUCCESS(rc), "how come?\n");
    memcpy(cbuf.getBase(), smallBuf, flength);
    cbuf.setOff(smallOffset);
    if (flength > smallOffset) {
	cbuf.setCnt(flength-smallOffset);
    } else {
	cbuf.setCnt(0);
    }
    bufferType = MMAP;
    rc = stub._ftruncate(flength);
    tassertMsg(_SUCCESS(rc), "failure\n");
    flengthFR = flength;
    if (smallBuf) {
	// This tells the server to blow away the buffer
	stub._syncSharedBuf(flength,0,0,1);
	smallBuf = NULL;
    }

#ifdef DILMA_DEBUG_SMALLFILES
    err_printf("OUT smallBufToMMap flength %ld off %ld Cnt %ld "
	       "flengthFR %ld\n",  flength, cbuf.getOff(), cbuf.getCnt(),
	       flengthFR);
#endif // #ifdef DILMA_DEBUG_SMALLFILES

    return rc;

}

/* virtual */ SysStatusUval
Buffer::readSmallBuffer(uval len, char * &buf, ThreadWait **tw)
{
    tassertMsg(bufferType == SMALL_BUFFER, "bufferType is %ld\n", bufferType);
    tassertMsg(smallBuf != NULL, "smallBuf NULL\n");
    tassertMsg(smallOffset >= 0 && smallOffset <= flength,
	       "smallOffset is %ld\n", smallOffset);
    len = MIN(len, flength - smallOffset);
    if (len > 0) {
	buf = &smallBuf[smallOffset];
	smallOffset += len;
	smallBufRefs++;
	return len;
    }
    buf = 0;
    return _SERROR(2241, FileLinux::EndOfFile, 0);
}

/* virtual */ SysStatusUval
Buffer::writeSmallBuffer(uval len, char * &buf, ThreadWait **tw)
{
    tassertMsg(bufferType == SMALL_BUFFER, "bufferType is %ld\n", bufferType);
    tassertMsg(smallBuf != NULL, "smallBuf NULL\n");
    tassertMsg((flength + len <= smallBufSize), "flength %ld, len %ld, "
	       "smallBufSize %ld\n", flength, len, smallBufSize);

    if (openFlags & O_APPEND) {
	smallOffset = flength;
    }

    if (len <= 0) {
	buf = 0;
	return 0;
    }

    buf = &smallBuf[smallOffset];
    uval end = MAX(smallBufModStart + smallBufModified, smallOffset + len);

    if (smallBufModified==0) {
	smallBufModStart = smallOffset;
    } else {
	smallBufModStart = MIN(smallOffset, smallBufModStart);
    }

    smallBufModified = MIN(end, smallBufSize) - smallBufModStart;

    smallOffset += len;
    smallBufRefs++;
    if (smallOffset > flength) {
	flength = smallOffset;
    }
    //err_printf("In locked_writeSmallBuffer flength is %ld\n", flength);
    return len;

}

SysStatusUval
Buffer::transportSmallBuf(uval op)
{
    passertMsg(flength <= smallBufSize, "flength %ld\n", flength);
    SysStatusUval rc;

    uval len = flength;
    uval load;
    uval offset = 0;

    while (len) {
	load = MIN(len, FileLinux::SMALL_FILE_PPC_MAX_LOAD);
	if (op == RCV) {
	    rc = stub._read(&smallBuf[offset], load, offset);
	} else {
	    tassertMsg(op == SEND, "?");
	    rc = stub._write(&smallBuf[offset], load, offset);
	}
	tassertMsg(_SUCCESS(rc) || _SGENCD(rc) == ENOENT,
		   "rc (%ld, ?, %ld)\n", _SERRCD(rc), _SGENCD(rc));
	_IF_FAILURE_RET(rc);
	// Notice we know for sure that the Server has flength bytes to send us.
	// It's trivial to do deal with the general case, but we want to know if
	// we change the system in a way that breaks this assumption
	tassertMsg(load == _SGETUVAL(rc),
		   "Assumption broke! load %ld, got %ld\n", load, rc);

	offset += _SGETUVAL(rc);
	len -= _SGETUVAL(rc);
    }

    return _SRETUVAL(flength);
}

/* virtual */ SysStatus
Buffer::afterServerUpdate()
{
    switch (bufferType) {
    case MMAP:
	flengthFR = flength;
	break;
    case SMALL_BUFFER:
	break;
    default:
	tassertMsg(0, "bufferType %ld\n", bufferType);
    }

    return 0;
}
