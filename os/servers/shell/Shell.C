/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Shell.C,v 1.67 2005/08/11 20:20:56 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <io/FileLinux.H>
#include "Shell.H"
#include <stdio.h>
#include <scheduler/Scheduler.H>
#include <sys/ProcessLinuxClient.H>

Shell::Shell()
{
    strcpy(prompt, "");
    cmdCount = 0;
    runningShell = 1;
    //path = NULL;
    //prompt = NULL;
}

void
Shell::shell(uval argc, const char *argv[])
{
    SysStatus rc;
    char inputCmdBuf[INPUT_CMD_BUF_SIZE];

    // set Umask so it's not garbage
    (void) FileLinux::SetUMask(0);

    if (argc > 1) {
	uval i;
	uval sum = 0;
	char *b;
	char *p;
	uval *len;

	--argc;
	len = (uval *)alloca(sizeof(uval) * argc);

	//size up the buffer
	for (i = 0; i < argc;  i++) {
	    len[i] = strlen(argv[i + 1]);
	    sum += len[i] + 1;
	}
	b = (char *)alloca(sum + 1);
	p = b;

	// concat the buffer
	for (i = 0; i < argc; i++) {
	    strncpy(p, argv[i + 1], len[i]);
	    p += len[i];
	    *p++ = ' ';
	}
	*p = '\0';

	rc = processCmdLine(b);
	printf("\n");
	if (_FAILURE(rc)) {
	    printf("command: \"%s\" failed with rc 0x%lx\n",
		       b, rc);
	}
	printf("\n");

	return;
    }
    // Continue on interactively

    // FIXME properly done with exit
    while (runningShell) {
	setPrompt();
	printf("%s", prompt);
	gets(inputCmdBuf);
	uval len = strlen(inputCmdBuf);
	if (len > 0) {
	    strcpy(hist[cmdCount%MAX_HISTORY_LEN], inputCmdBuf);
	    cmdCount++;
	    rc = processCmdLine(inputCmdBuf);
	    printf("\n");
	    if (rc != 0) {
		printf("command failed with rc %ld\n", rc);
	    }
	    printf("\n");
	}
    }
}

#define MAXARGC 20
SysStatus
Shell::processCmdLine(char *cmdLine)
{
    char *argv[MAXARGC];
    char *const envp[] = {NULL};
    uval argc;
    SysStatus rc;
    int c;
    enum {SKIPWS, FINDWS} state = SKIPWS;
    uval wait;

    argc = 0;

    // really simple and brain dead
    while ((c = *cmdLine) != '\0') {
	// need to leave room for the last NULL pointer
	if (argc > MAXARGC - 2) {
	    printf("Sorry, too many args: Max is %d\n", MAXARGC);
	    return -1;
	}
	switch (c) {
	case ' ':
	case '\t':
	    if (state != SKIPWS) {
		*cmdLine = '\0';
		state = SKIPWS;
	    }
	    break;
	default:
	    if (state == SKIPWS) {
		argv[argc] = cmdLine;
		++argc;
		state = FINDWS;
	    }
	    break;
	}
	++cmdLine;
    }

    if (argc == 0) {
	return 1;
    }

    if (argc > 1 &&
	argv[argc - 1][0] == '&' && argv[argc - 1][1] == '\0') {
	argc--;
	wait = 0;
    } else {
	wait = 1;
    }

    argv[argc] = NULL;

    if (doBuiltInCmds(argc, argv) != 0) {
	if (doExecutable(argc, argv) != 0) {
	    pid_t childLinuxPID;
	    sval status;

	    rc = ProgExec::ForkProcess(childLinuxPID);
	    if (_FAILURE(rc)) {
		err_printf("oops2 %lx\n", rc);
		return rc;
	    }
	    if (childLinuxPID == 0) {
		ProgExec::ArgDesc *args;
		rc = ProgExec::ArgDesc::Create(argv[0], argv, envp, args);
		if (_SUCCESS(rc)) rc = runExecutable(args, wait);
		args->destroy();
		if (_FAILURE(rc)) {
		    printf("%s: Command not found\n", argv[0]);
		    DREFGOBJ(TheProcessLinuxRef)->exit(rc);
		    // NOTREACHED
		    tassertMsg(0, "Returned from exit()!\n");
		}
		DREFGOBJ(TheProcessRef)->kill();
	    }
	    // wait for our child
	    rc = ProcessLinuxClient::WaitPIDInternal(childLinuxPID, status, 0);
	    tassertMsg(_SUCCESS(rc), "WaitPIDInternal() failed.\n");
	}
    }
    return 0;
}

void
Shell::setPrompt()
{
    SysStatus rc;
    char pathBuf[PATH_MAX+1];
    char cwdBuf[PATH_MAX+1];
    PathNameDynamic<AllocGlobal> *cwd = (PathNameDynamic<AllocGlobal>*) pathBuf;
    uval cwdlen;

    rc = FileLinux::Getcwd(cwd, sizeof(pathBuf));
    tassert(_SUCCESS(rc), err_printf("can't get CWD\n"));

    cwdlen = _SGETUVAL(rc);

    rc = cwd->getUPath(cwdlen, cwdBuf, PATH_MAX+1);

    tassert(_SUCCESS(rc), err_printf("can't parse CWD\n"));

    sprintf(prompt, "k42:%s[%ld] > ", cwdBuf, cmdCount);
}


SysStatus
Shell::doExecutable(uval argc, char *argv[])
{

    if (0) {
	printf("printf found built in command %s\n", argv[0]);
	return 0;
    }

    return 1;
}
