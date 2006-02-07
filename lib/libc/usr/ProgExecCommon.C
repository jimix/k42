/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExecCommon.C,v 1.9 2005/01/08 16:16:55 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for user-level initialization
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stub/StubObj.H>
#include <usr/ProgExec.H>
#include <stub/StubFRCRW.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessServer.H>
#include <defines/paging.H>
#include <trace/traceMem.h>

//
// Map a single binary module.  All we need to know about the module,
// FR, sections and offsets is inside info.
// NB: targetXH==0 -> mapping is being done within the current process.
// We don't know how to tolerate failures here, so we use passerts.
//
/*static*/ SysStatus
ProgExec::MapModule(BinInfo *info, XHandle targetXH, MemRegion *region,
	       uval textType, RegionType::Type keepOnExec)
{
    SysStatus rc;

    /*
     * for historical reasons :-), we don't just report data and bss
     * facts.  On return,
     * bssStart = dataStart+dataSize
     * totalDataSize = dataSize+bssSize
     *
     * totalBSS start and size describe the remaining BSS region
     * if one is needed.
     */

    //N.B. some of these values are rounded to page boundaries
    //     or otherwise fudged - read carefully
    uval textRegionStart, textRegionSize,
	dataRegionStart, dataRegionSize, dataRegionEnd,
	bssRegionStart, bssRegionSize;

    textRegionStart = info->seg[info->textSegIndex].vaddr -
	info->seg[info->textSegIndex].offset;

    tassertMsg(info->seg[info->textSegIndex].memsz ==
	       info->seg[info->textSegIndex].filesz, "text has bss?\n");

    textRegionSize =
	PAGE_ROUND_UP(info->seg[info->textSegIndex].vaddr +
		      info->seg[info->textSegIndex].memsz) - textRegionStart;
    

    // better be page aligned
    if (textRegionStart & (PAGE_SIZE-1)) {
	return _SERROR(2336, 0, EINVAL);
    }

    // round ends up and down to page boundary, fixing size
    dataRegionStart = PAGE_ROUND_DOWN(info->seg[info->dataSegIndex].vaddr);
    dataRegionEnd = info->seg[info->dataSegIndex].vaddr
	+ info->seg[info->dataSegIndex].filesz;

    // N.B. it may be that bss fits on the last page of data
    // and we don't need a bss region
    bssRegionStart = PAGE_ROUND_UP(dataRegionEnd);
    dataRegionSize = bssRegionStart - dataRegionStart;
    bssRegionSize = info->seg[info->dataSegIndex].memsz
	- info->seg[info->dataSegIndex].filesz;

    if (bssRegionSize > (bssRegionStart - dataRegionEnd)) {
	//Subtract bss on last page of data to get additional mapping needed
	bssRegionSize -= (bssRegionStart - dataRegionEnd);
    } else {
	//Indicate no additional bss mapping needed
	bssRegionSize = 0;
    }

    if (((textRegionStart & (SEGMENT_SIZE-1)) == 0) &&
       (textRegionStart + ALIGN_UP(textRegionSize,SEGMENT_SIZE)
	<= dataRegionStart)) {
	//starts on segment boundary, won't overlap data, so
	//make it segment sized to allows shared segment mappings
	textRegionSize = ALIGN_UP(textRegionSize,SEGMENT_SIZE);
    }

    ObjectHandle imageFRCRWOH;
    // Maybe Create a copy on reference copy of the image file
    rc = StubFRCRW::_Create(imageFRCRWOH, info->localFR);

    passertMsg(_SUCCESS(rc),"MapModule failure: %lx\n",rc);

    AccessMode::mode textMode, dataMode;
    RegionType::Type type;
    /*
     *FIXME we always set text writable so debugger can set
     *breakpoints.  We should respect flags and have a debugger
     *service to get write access when needed
     */
    tassertMsg(info->seg[info->textSegIndex].flags & PF_X,
	       "text segment not set executable\n");
    textMode = AccessMode::mode(
	(uval)(AccessMode::writeUserWriteSup)+ (uval)(AccessMode::execute));
    /*
     * if text segment is really writable, we must fork copy it
     * if not, it is more efficient to make a new copy on write copy
     * also, forkcopy causes problems if the debugger has set breakpoints
     */
    type = (info->seg[info->textSegIndex].flags & PF_W) ?
	RegionType::ForkCopy : RegionType::NewCRW;

    rc = StubRegionDefault::_CreateFixedAddrLenExt(
	textRegionStart, textRegionSize,
	imageFRCRWOH, 0, textMode, targetXH, type + keepOnExec);

    Obj::ReleaseAccess(imageFRCRWOH);

    passertMsg(_SUCCESS(rc),"MapModule failure: %lx\n",rc);

    ObjectHandle dataFROH;
    ObjectHandle largeFROH;  // could overload dataFROH but this is clearer

    if (region[textType+1].pageSize == 0) {
	// if the page size was unspecified it's going to be default size
	region[textType+1].pageSize = PAGE_SIZE;
    }

    if (region[textType+1].pageSize != PAGE_SIZE) {
	// sort of a kludge, if the user has asked for large pages but
	// has not linked the executable with the data section in a
	// separate segment we going print a warning and unset the page size
	if ((textRegionStart >> 27) == (dataRegionStart >> 27)) {
	    err_printf("Warning: Large data segment requested but executable not\n");
	    err_printf("    linked to support different segments ignoring env var\n");
	    region[textType+1].pageSize = PAGE_SIZE;	
	}
    }

    if (region[textType+1].pageSize == PAGE_SIZE) {
	rc = StubFRCRW::_Create(dataFROH, info->localFR);
    } else {
	// large pages
	passertMsg(region[textType+1].pageSize == LARGE_PAGES_SIZE,
		   "Requested size %lx, K42 only supports large pages of size %x\n",
		   region[textType+1].pageSize, LARGE_PAGES_SIZE);
	rc = StubFRComputation::_CreateLargePage(largeFROH,
						 region[textType+1].pageSize);
	if(_FAILURE(rc)) {
	    err_printf(
		"Failed to make large page FR for data region %lx\n", rc);
	}
    }

    tassertMsg(info->seg[info->dataSegIndex].flags & PF_W,
	       "data segment not set writable\n");
    /*
     * 32 bit GOT code has instructions in the data and/or bss segments and
     * requests executable permissions
     */
    dataMode = AccessMode::mode((uval)(AccessMode::writeUserWriteSup) +
	((info->seg[info->dataSegIndex].flags & PF_X) ?
	 (uval)(AccessMode::execute) : 0));
    type = RegionType::ForkCopy;

    tassertMsg((dataRegionSize & (PAGE_SIZE-1)) == 0,
	       "should be page aligned\n");
    //The following invokes the create method but does NOT create a stub object.
    if (region[textType+1].pageSize == PAGE_SIZE) {
	rc = StubRegionDefault::_CreateFixedAddrLenExt(
	    dataRegionStart, dataRegionSize, dataFROH,
	    PAGE_ROUND_DOWN(info->seg[info->dataSegIndex].offset),
	    dataMode, targetXH, type + keepOnExec);
	Obj::ReleaseAccess(dataFROH);
    } else {
	// large pages
	// because we can no map in two regions with the same FR for large
	//  pages we create one region here big enough for both data and bss
	uval size;
	size = PAGE_ROUND_UP(dataRegionSize) + PAGE_ROUND_UP(bssRegionSize);

	rc = StubRegionDefault::_CreateFixedAddrLenExt(
	    dataRegionStart, size, largeFROH,
	    PAGE_ROUND_DOWN(info->seg[info->dataSegIndex].offset),
	    dataMode, targetXH, type + keepOnExec);
	// do not release OH yet because we'll use for bss
	// we now need to get the data into out large page region
	// map it in copy and then delete the region
	
	uval start;
	start = 0;

	rc = StubRegionDefault::_CreateFixedLenExt(
		start, dataRegionSize, 0, info->localFR, 
		PAGE_ROUND_DOWN(info->seg[info->dataSegIndex].offset),
		AccessMode::writeUserWriteSup, 0, RegionType::UseSame);
	passertMsg(_SUCCESS(rc),"Failed to map data seg for read: %lx\n",rc);
	memcpy((void *)dataRegionStart, (void *)start, dataRegionSize);
	rc = DREFGOBJ(TheProcessRef)->regionDestroy(start);
	tassertMsg(_SUCCESS(rc),"Failed to destroy region: %lx\n",rc);
	Obj::ReleaseAccess(largeFROH);
    }
    passertMsg(_SUCCESS(rc),"MapModule failure for data: %lx\n",rc);
    TraceOSMemData((uval64)dataRegionStart, (uval64)dataRegionSize);


    // If we're loading a module for the current process, and it has
    // a bss region --> there's a bit of bss at the end of data for us
    // to clear

    if (targetXH==0) {
	// Clear end of page of data segment
	// Note that this takes care of any BSS in the last page of data
	memset((void*)dataRegionEnd, 0, bssRegionStart - dataRegionEnd);
    }

    ObjectHandle bssFROH;

    if (bssRegionSize > 0) {
	if (region[textType+1].pageSize == PAGE_SIZE) {
	    rc = StubFRComputation::_Create(bssFROH);
	    rc = StubRegionDefault::_CreateFixedAddrLenExt(
		bssRegionStart,  PAGE_ROUND_UP(bssRegionSize), bssFROH,
		0, dataMode, targetXH,
		RegionType::ForkCopy + keepOnExec);

	    Obj::ReleaseAccess(bssFROH);
	} else {
	    // nothing we've already done see comment in creation of data reg
	}
    }

    passertMsg(_SUCCESS(rc),"MapModule failure for bss: %lx\n",rc);
    TraceOSMemBSS((uval64)bssRegionStart, (uval64)bssRegionSize);

    // record information about regions
    region[textType].start = textRegionStart;
    region[textType].size = textRegionSize;
    region[textType].offset = 0;
    region[textType].baseFROH = info->frOH;

    region[textType+1].start = dataRegionStart;
    region[textType+1].size = dataRegionEnd - dataRegionStart;
    region[textType+1].offset=
	PAGE_ROUND_DOWN(info->seg[info->dataSegIndex].offset);
    region[textType+1].baseFROH.init();

    region[textType+2].start = bssRegionStart;
    region[textType+2].size = bssRegionSize;
    region[textType+2].offset = 0;
    region[textType+2].baseFROH.init();



    return 0;
}

// Due to variable length allocation in ArgDesc::Create, cleaning
// up after this isn't going to be pretty either.
void
ProgExec::ArgDesc::destroy() {
    if(prefixDesc) prefixDesc->destroy();
    freePinnedGlobalPadded(this, bufSize);
}

SysStatus
ProgExec::ArgDesc::setArgvPrefix(const char* const argv[])
{
    tassertMsg(prefixDesc == 0, "Prefix already set\n");
    char* emptyEnv = 0;
    return Create("", argv, &emptyEnv, prefixDesc);
}

//Build something that describes argv/envp in safe, known memory
template<typename POINTER>
SysStatus
__Create(const char* fileName, POINTER argv[],
	 POINTER envp[],ProgExec::ArgDesc*&ptr)
{
    uval size = 0;
    uval argc = 0;
    uval fileNameLen = strlen(fileName) + 1;
    while (argv[argc]) {
	size += strlen((char*)(uval)(argv[argc]))+1;
	++argc;
    }
    uval envc=0;
    while (envp[envc]) {
	size += strlen((char*)(uval)(envp[envc]))+1;
	++envc;
    }
    size+= ((fileNameLen + (argc+1) + (envc+1)) * sizeof(char*)) +
	sizeof(ProgExec::ArgDesc);
    ptr = (ProgExec::ArgDesc*)allocPinnedGlobalPadded(size);
    memset(&ptr->prog, 0, sizeof(ptr->prog));

    ptr->bufSize = size;
    ptr->argc = argc;
    ptr->envc = envc;
    ptr->fileNameLen = fileNameLen;
    ptr->prefixDesc = 0;
    char** vector = (char**)&ptr->args[0];
    char*  string = (char*)&vector[1 + argc+1 + envc+1];

    // copy the filename in front to argv[]

    *vector = string;
    strcpy(string, fileName);
    string += ptr->fileNameLen;
    ++vector;

    for (uval i=0; i<argc; ++i) {
	*vector = string;
	strcpy(string, (char*)(uval)(argv[i]));
	string += strlen(string)+1;
	++vector;
    }
    *vector = NULL;
    ++vector;
    for (uval i=0; i<envc; ++i) {
	*vector = string;
	strcpy(string, (char*)(uval)(envp[i]));
	string += strlen(string)+1;
	++vector;
    }
    *vector = NULL;
    ++vector;
    return 0;
}

/*static*/ SysStatus
ProgExec::ArgDesc::Create(const char* fileName, const char* const argv[],
			  const char* const envp[], ArgDesc*&ptr)
{
    return __Create<uval>(fileName, (uval* )argv, (uval*) envp, ptr);
}

/*static*/ SysStatus
ProgExec::ArgDesc::Create32(const char* fileName, const char* const argv[],
			  const char* const envp[], ArgDesc*&ptr)
{
    return __Create<uval32>(fileName, (uval32*)argv, (uval32*)envp, ptr);
}
