/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: userRunProcess.C,v 1.82 2004/10/17 16:26:30 bob Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <stub/StubObj.H>
#include <stub/StubRegionDefault.H>
#include <io/FileLinux.H>
#include <alloc/PageAllocatorUser.H>
#include <sys/BaseProcess.H>
#include <cobj/XHandleTrans.H>
#include <sys/Dispatcher.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <usr/ProgExec.H>
#include <mem/Access.H>
#include "io/PathName.H"
#include <sys/memoryMap.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <usr/runProcessCommon.H>
#include <misc/baseStdio.H>

SysStatus
shellInterp(char* imageStart, uval sz, ProgExec::ArgDesc* argDesc)
{
    SysStatus rc = 0;
    uval i;
    int c;
    char **newArgv;
    char *p;
    uval argc;
    if (imageStart[0] != '#' || imageStart[1] != '!') {
	return _SERROR(2193, 0, ENOEXEC);
    }

    // find EOL
    i = 2;
    // mark for second pass
    p = &imageStart[i];
    argc = 0;
    uval skip_ws = 1;
    while (i < sz &&
	   ((c = imageStart[i++]) != '\0' && c != '\n')) {
	if (c == ' ' || c == '\t') {
	    if (skip_ws == 0) {
		skip_ws = 1;
	    }
	} else {
	    if (skip_ws == 1) {
		++argc;
		skip_ws = 0;
	    }
	}
    }
    if (i >= sz) {
	tassertWrn(0, "no EOL while reading interpreter\n");
	return _SERROR(2194, 0, ENOEXEC);
    }
    p = (char*)alloca(i+1);
    memcpy(p, imageStart+2, i-2);
    // mark EOL with nil
    p[i-3] = '\0';

    // allocate the pointers requires for newArgv including an extra NULL
    newArgv = (char **) alloca(sizeof(char *) * (argc + 1));

    // skip spaces and tabs..
    while (*p != '\0' &&
	   (*p == ' ' || *p == '\t')) {
	    ++p;
    }
    if (*p == '\0') {
	tassertWrn(0, "fail to read interpreter\n");
	return _SERROR(2195, 0, ENOEXEC);
    }

    i = 0;
    while (*p != '\0') {
	newArgv[i++] = p;
	// get end of arg
	while (*p != '\0' &&
	       (*p != ' ' && *p != '\t')) {
	    ++p;
	}
	if (*p != '\0') {
	    *p++ = '\0';
	    // skip spaces and tabs..
	    while (*p != '\0' &&
		   (*p == ' ' || *p == '\t')) {
		++p;
	    }
	}
    }

    newArgv[i] = NULL;

    //Can't keep argv in alloca/stack memory,
    //use ArgDesc structure instead
    rc = argDesc->setArgvPrefix(newArgv);
    if(_SUCCESS(rc)) rc = argDesc->setArg0FromPath();

    return (rc);
}


SysStatusProcessID
runExecutable(ProgExec::ArgDesc *args, uval wait)
{
    SysStatus rc;
    uval imageStart;
    ObjectHandle imageFROH;
    FileLinuxRef fileRef = 0;
    
    rc = RunProcessOpen(args->getFileName(), &imageStart, &imageFROH, &fileRef);
    if (_FAILURE(rc)) return rc;

    SysStatusProcessID myPID = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(myPID), err_printf("woops\n"));

    // Figure out cwd
    char cwdBuf[PATH_MAX+1];
    uval cwdLen;
    char cwd[PATH_MAX+1];
    PathNameDynamic<AllocGlobal> *cwdPath;
    cwdPath = (PathNameDynamic<AllocGlobal>*) cwdBuf;

    rc = FileLinux::Getcwd(cwdPath, sizeof(cwdBuf));
    if (_FAILURE(rc)) return rc;

    cwdLen = _SGETUVAL(rc);

    rc = cwdPath->getUPath(cwdLen, cwd, sizeof(cwd));
    if (_FAILURE(rc)) return rc;

    rc = RunProcessCommon(args->getFileName(), imageStart, imageFROH,
			  args, cwd, _SGETPID(myPID), wait);
    if (_FAILURE(rc)) {
	char interp[4096];
	uval interpSize;
	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(fileRef)->read(interp, sizeof(interp), &tw, moreAvail);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		interpSize = _SGETUVAL(rc);
		break;
	    }
	}

	if (_FAILURE(rc)) {
	    tassertWrn(0, "fail to read bytes from command file\n");
	    return _SERROR(2196, 0, ENOEXEC);
	}

	DREF(fileRef)->detach();

	rc = shellInterp(interp, interpSize, args);

	if (_SUCCESS(rc)) {
	    if (traceUserEnabled()) {
		char llengths[1024];
		char lengthstr[64];
		char largv[1024];
		uval largvlen, llengthlen, i, argvilen, lengthstrlen;
		char** argv=args->getArgv();
		largvlen = 0;
		llengthlen = 0;
		argvilen = 0;
		i = 0;
		largv[0] = '\0';
		lengthstr[0] = '\0';
		while (argv[i]) {
		    argvilen = strlen(argv[i]);
		    // +1 for space
		    if ((argvilen + largvlen + 1) > 1024) break;

		    baseSprintf(lengthstr, "%ld", argvilen);
		    lengthstrlen = strlen(lengthstr);
		    // +1 for space
		    if ((lengthstrlen + llengthlen + 1) > 1024) break;

		    // add spaces if not first thing
		    if (i!=0) {
			strncpy(largv+largvlen, " ", 1);
			strncpy(llengths+llengthlen, " ", 1);
			llengthlen++;
			largvlen++;
		    }

		    strncpy(largv+largvlen, argv[i], argvilen);
		    strncpy(llengths+llengthlen, lengthstr, lengthstrlen);
		    llengthlen += lengthstrlen;
		    largvlen += argvilen;

		    i++;
		    largv[largvlen] = '\0';
		    llengths[llengthlen] = '\0';
		}
		TraceOSUserArgv(llengths, largv);
	    }


	    //Is script, argv has been adjusted
	    rc = RunProcessOpen(args->getArgvPrefix()[0], &imageStart,
				&imageFROH, &fileRef);

	    if (_SUCCESS(rc)) {
		rc = RunProcessCommon(args->getArgvPrefix()[0],
				      imageStart, imageFROH,
				      args, cwd, _SGETPID(myPID), wait);
	    }
	}
    }

    if (_FAILURE(rc) && fileRef) {
	DREF(fileRef)->detach();
    }

    return rc;
}

SysStatus
RunProcessOpen(const char *name, uval *imageStart, ObjectHandle *imageFROH,
	       FileLinuxRef *fileRef)
{
    SysStatus rc;
    uval fileStart=0, fileSize;
    ObjectHandle fileFROH;

    FileLinuxRef flr;
    rc = FileLinux::Create(flr, name, O_RDONLY, 0);
    _IF_FAILURE_RET(rc);

    // check the file
    FileLinux::Stat statBuf;
    rc = DREF(flr)->getStatus(&statBuf);
    if (_FAILURE(rc)) {
	DREF(flr)->destroy();
	tassertWrn(0, "stat failed\n");
	return rc;
    }

    //FIXME This is redundant since we cannot get an FR from anything
    //but a regular file
    if (! S_ISREG(statBuf.st_mode)) {
	DREF(flr)->destroy();
	tassertWrn(0, "not a regular file %s\n", name);
	return _SERROR(1557, 0, ENOEXEC);
    }

    // FIXME check permissions better than this
    if (! (statBuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) {
	DREF(flr)->destroy();
	tassertWrn(0, "no execution permissions for %s\n", name);
	return _SERROR(1558, 0, EACCES);
    }
    // setuid/setgid?
    if (statBuf.st_mode & S_ISUID) {
	rc = DREFGOBJ(TheProcessLinuxRef)->insecure_setuidgid(statBuf.st_uid,
		(gid_t)-1);
    }

    if (statBuf.st_mode & S_ISGID) {
	rc = DREFGOBJ(TheProcessLinuxRef)->insecure_setuidgid((uid_t)-1,
	    statBuf.st_gid);
    }

    rc = DREF(flr)->getFROH(fileFROH, FileLinux::DEFAULT);

    if (_FAILURE(rc)) {
	DREF(flr)->detach();
	tassertWrn(0, "get getFROH failed\n");
	return rc;
    }

    fileSize = statBuf.st_size; // page rounding done by FR

    // round up size to allow shared segment mapping
    rc = StubRegionDefault::_CreateFixedLenExt(
	fileStart,ALIGN_UP(fileSize,SEGMENT_SIZE),
	0, fileFROH, 0,
	(uval)(AccessMode::readUserReadSup), 0,
	RegionType::FreeOnExec);

    if (_FAILURE(rc)) {
	DREF(flr)->detach();
	Obj::ReleaseAccess(fileFROH);
    } else {
	*imageFROH = fileFROH;
	*imageStart = fileStart;
	if (fileRef) {
	    *fileRef = flr;
	} else {
	    DREF(flr)->detach();
	}
    }

    return rc;
}

