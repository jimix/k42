/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernRunProcess.C,v 1.169 2004/04/28 23:05:44 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: establish and call a user process
 * **************************************************************************/
#include "kernIncs.H"
#include <sys/ppccore.H>
#include <sys/baseBootServers.H>
#include "proc/Process.H"
#include "mem/FR.H"
#include "mem/Region.H"
#include "mem/RegionDefault.H"
#include "mem/HATDefault.H"
#include "mem/FCMDefault.H"
#include "mem/FRPlaceHolder.H"
#include "mem/FCMStartup.H"
#include "mem/Access.H"
#include "proc/Process.H"
#include <cobj/TypeMgr.H>
#include "mem/PageAllocatorKern.H"
#include <sys/Dispatcher.H>
#include <sys/memoryMap.H>
#include "kernRunProcess.H"
#include "meta/MetaFileLinuxServer.H"
#include <usr/ProgExec.H>
#include <usr/runProcessCommon.H>
#include <cobj/XHandleTrans.H>

extern bootServerHeader bootServers[];

//
// Because we can't access file-systems, we have to special-case the
// startup for baseServers: it doesn't have an exec.so.
// For any other process, because the kernel can't find exec.so, we start
// baseServers, with the argv of the process we really want to start
//
SysStatus
kernRunInternalProcess(char *name, char *arg1, char *arg2, uval wait)
{
    SysStatus rc;

    // open all the objects on my behalf, then transfer ownership
    // in child routine
    SysStatusProcessID mypid = DREFGOBJK(TheProcessRef)->getPID();

    char *progName="baseServers";

    char *argv[] = {name, arg1, arg2, NULL};
    char *envp[] = {NULL};

    uval imageStart;
    ObjectHandle imageFROH;
    rc = RunProcessOpen(progName, &imageStart, &imageFROH, 0);
    _IF_FAILURE_RET(rc);

    ProgExec::ArgDesc *argDesc;

    rc = ProgExec::ArgDesc::Create(progName, argv, envp, argDesc);
    if(_SUCCESS(rc)) {
	rc = RunProcessCommon(progName, imageStart, imageFROH,
			  argDesc, NULL, _SGETPID(mypid), wait);
	argDesc->destroy();
    }
    return rc;
}

SysStatus
StartDefaultServers()
{
    extern uval useLongExec;
    useLongExec = 1;
    err_printf("---- starting first server\n");
    kernRunInternalProcess("baseServers", NULL, NULL, 0);
    err_printf("---- done starting first server\n");
    return 0;
}

SysStatus
RunProcessOpen(const char *name, uval *imageStart,
	       ObjectHandle *imageFROHArg,
	       FileLinuxRef *fileRef)
{
    SysStatus rc;
    tassertMsg(fileRef == 0, "fileref not supported in kernel\n");
    char *origName = (char*)alloca(strlen(name)+1);
    strcpy(origName, name);

    bootServerHeader *hdr = NULL;
    uval imageVirt, imageReal, imageSize;

    while (name) {
	for (hdr = bootServers;
	     (hdr->offset() != 0) && (strcmp(hdr->name(), name) != 0);
	     hdr++) {
	    continue;
	}

	if (hdr->offset()) {
	    break;
	}

	// strip off left-most directory, try again
	name = strchr(name, '/');
	if (name) ++name;
    }

    if (hdr->offset() == 0) {
	cprintf("runInternalProcess:  \"%s\" not found.\n", origName);
	return -1;
    }

    imageVirt = ((uval) bootServers) + hdr->offset();
    imageSize = PAGE_ROUND_UP(hdr->size());
    imageReal = PageAllocatorKernPinned::virtToReal(imageVirt);

    // open all the objects on my behalf, then transfer ownership
    // in child routine
    SysStatusProcessID mypid = DREFGOBJK(TheProcessRef)->getPID();

    FRRef frStartRef;
    FCMRef fcmRef;
    RegionRef regRef;
    ObjectHandle imageFROH;
    uval fcmImageOff;

    tassert((imageVirt == PAGE_ROUND_UP(imageVirt)),
	    err_printf("image not starting on page boundary %lx\n",imageVirt));
    fcmImageOff = PageAllocatorKernPinned::virtToReal(imageVirt);
    rc = FCMStartup::Create(fcmRef, fcmImageOff, imageSize);

    FRPlaceHolder::Create(frStartRef);

    // connect fr and fcm
    DREF(frStartRef)->installFCM(fcmRef);

    rc = DREF(frStartRef)->giveAccessByServer(imageFROH, mypid);

    rc = RegionDefault::CreateFixedLen(regRef,
				       DREFGOBJK(TheProcessRef)->getRef(),
				       imageVirt,
				       imageSize, PAGE_SIZE, frStartRef, 0, 0,
				       AccessMode::noUserWriteSup,
				       RegionType::K42Region);

    *imageStart = imageVirt;
    *imageFROHArg = imageFROH;
    return rc;
}
