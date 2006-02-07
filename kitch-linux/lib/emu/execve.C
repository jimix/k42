/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: execve.C,v 1.37 2005/03/22 21:38:44 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: execve(2) system call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define execve __k42_linux_execve
#include <unistd.h>
#include <stdlib.h>		// for exit()
#include "FD.H"
#include <usr/ProgExec.H>
#include <usr/runProcessCommon.H>
#include <sys/ProcessLinux.H>
#include <trace/traceUser.h>
#include <sys/ProcessWrapper.H>
#include <misc/baseStdio.H>

SysStatus
execve_common(ProgExec::ArgDesc *argDesc);

int
execve(const char *filename, char *const argv[], char *const envp[])
{
#undef execve
    SysStatus rc;
    //FIXME: this should have its own PROPER implementation. replacing
    //the process id, returning what the new executable returned, etc.
    int ret;

    if (filename == NULL) {
	return -EFAULT;
    }

    if (useLongExec) {
	ret = __k42_linux_spawn(filename, argv, envp, 0);

	if (ret >= 0) {
	    DREFGOBJ(TheProcessRef)->kill();
	}
	return ret;
    }
    SYSCALL_ENTER();
    ProgExec::ArgDesc* argDesc;
    rc = ProgExec::ArgDesc::Create(filename, argv, envp, argDesc);

    if(_SUCCESS(rc)) {
	rc = execve_common(argDesc);
    }

    //Normally does not return

    SYSCALL_EXIT();
    return -_SGENCD(rc);
}

extern "C" int
__k42_linux_execve_32(const char *filename, char *const argv[], 
                      char *const envp[])
{
    SysStatus rc;
    //FIXME: this should have its own PROPER implementation. replacing
    //the process id, returning what the new executable returned, etc.

    if (filename == NULL) {
	return -EFAULT;
    }

    passertMsg(!useLongExec, "marc was lazy - 32 bit LongExec NYI\n");
#if 0
    // need a 32 bit spawn, thus must change runExecutable to take argdesc
    int ret;
    if (useLongExec) {
	ret = __k42_linux_spawn(filename, argv, envp, 0);

	if (ret >= 0) {
	    DREFGOBJ(TheProcessRef)->kill();
	}
	return ret;
    }
#endif
    SYSCALL_ENTER();
    ProgExec::ArgDesc* argDesc;
    rc = ProgExec::ArgDesc::Create32(filename, argv, envp, argDesc);

    if(_SUCCESS(rc)) rc = execve_common(argDesc);

    //Normally does not return

    SYSCALL_EXIT();
    return -_SGENCD(rc);
}

SysStatus
execve_common(ProgExec::ArgDesc *argDesc)
{
    // do loop with one iteration to allow break instead of goto on error
    ProgExec::ExecInfo newProg;
    FileLinuxRef flr = NULL;
    SysStatus rc;
    
    do {
	uval imageStart;
	ObjectHandle frOH;
	memset(&newProg, 0, sizeof(newProg));

	// First, get filename's absolute path
	// FIXME: We need to unreference symlinks!
	PathNameDynamic<AllocGlobal> *pathName;
	uval pathLen, maxPathLen;
	char absfilename[128];

	rc = FileLinux::GetAbsPath(argDesc->getFileName(),
				   pathName, pathLen, maxPathLen);
	if (_FAILURE(rc)) break;

	rc = pathName->getUPath(pathLen, absfilename, 128);
	if (_FAILURE(rc)) break;

	rc = RunProcessOpen(absfilename, &imageStart, &frOH, &flr);
	if (_FAILURE(rc)) break;

	rc = ProgExec::ParseExecutable(imageStart, frOH, &newProg.prog);
	if (_FAILURE(rc)) {
	    // Try it as a shell script
	    // Looking for shell script will create the right
	    // ArgDesc structure
	    rc = shellInterp((char*)imageStart, PAGE_SIZE, argDesc);

	    if (flr) {
		DREF(flr)->detach();
		flr = NULL;
	    }
	    RunProcessClose(newProg.prog);

	    if (_FAILURE(rc)) break;
	    rc = RunProcessOpen(argDesc->getArgvPrefix()[0],
				&imageStart, &frOH, 0);

	    if (_FAILURE(rc)) break;

	    rc = ProgExec::ParseExecutable(imageStart, frOH, &argDesc->prog.prog);

	    if (_FAILURE(rc)) break;

	} else {
	    memcpy(&argDesc->prog, &newProg, sizeof(argDesc->prog));
	}

	if (argDesc->prog.prog.interpOffset) {
	    uval interpName = imageStart + argDesc->prog.prog.interpOffset;
	    uval interpreterImageStart;
	    rc = RunProcessOpen((const char*)interpName,
				&interpreterImageStart, &frOH, 0);

	    // fixme cleanup needed
	    if (_FAILURE(rc)) break;

	    rc = ProgExec::ParseExecutable(interpreterImageStart, frOH,
					   &argDesc->prog.interp);
	    if (_FAILURE(rc)) break;

	}

	rc = ProgExec::ConfigStack(&argDesc->prog);
	if (_FAILURE(rc)) break;


	argDesc->prog.stackFR = argDesc->prog.localStackFR;


	// argv, envp are probably in memory that will be blown away.
	// Make a copy of that stuff here into memory that is safe.

	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    uval i =0;
	    char buf[1024];
	    char* next=buf;
	    char** argv=argDesc->getArgv();
	    int len = 0;
	    while (argv[i] &&  next< buf+510 && i<5) {
		len = strlen(argv[i]);
		if (len>16) len=16;
		if (len + next > buf+510) break;
		memcpy(next, argv[i], len);
		next+=len;
		*next = ' ';
		++next;
		*next=0;
		++i;
	    }
	    pid_t lpid;
	    DREFGOBJ(TheProcessLinuxRef)->getpid(lpid);
	    err_printf("Re-Mapping program %s (%s), k42 pid 0x%lx"
		       " linux pid 0x%x.\n",
		       absfilename, buf,
		       DREFGOBJ(TheProcessRef)->getPID(), lpid);
	}
	if (traceUserEnabled()) {
	    char llengths[1024];
	    char lengthstr[64];
	    char largv[1024];
	    uval largvlen, llengthlen, i, argvilen, lengthstrlen;
	    char** argv=argDesc->getArgv();
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

	TraceOSUserStartExec(DREFGOBJ(TheProcessRef)->getPID(),
		      (const char*)absfilename);
	ProgExec::UnMapAndExec(argDesc);

	passertMsg(0, "Shouldn't get here!!!\n")
    } while (0);


    if (flr) DREF(flr)->detach();
    if (argDesc) {
	RunProcessClose(argDesc->prog.prog);
	RunProcessClose(argDesc->prog.interp);
	argDesc->destroy();
    } else {
	RunProcessClose(newProg.prog);
	RunProcessClose(newProg.interp);
    }

    return rc;
}
