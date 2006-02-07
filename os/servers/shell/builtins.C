/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: builtins.C,v 1.116 2005/08/11 20:20:56 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user-level shell built in commands
 * **************************************************************************/

#include "sys/sysIncs.H"
#include <sys/BaseProcess.H>
#include "Shell.H"
#include <io/FileLinux.H>
#include <stdio.h>
#include <sys/TimerEvent.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/ppccore.H>
#include <scheduler/Scheduler.H>
#include <fcntl.h>
#include <stub/StubKBootParms.H>
//FIXME HUGE HACK!!
#include "../../../lib/libc/usr/GDBIO.H"

//FIXME should get this from ThinIP.H
#define MAX_BUF_SIZE 1024

extern FileLinuxRef tty;

typedef uval (*SHFUNC)(uval, char *[]);

typedef struct {
    char *name;
    SHFUNC func;
} CmdRec;

static uval logout(uval, char *[]);
static uval echo(uval, char *[]);
static uval ls(uval, char *[]);
static uval cat(uval, char *[]);
static uval dir(uval, char *[]);
static uval history(uval, char *[]);
static uval breakcmd(uval, char *[]);
static uval marccmd(uval, char *[]);
static uval debug(uval, char *[]);
static uval trace_print(uval, char *[]);
static uval login(uval, char *[]);

extern Shell shell;

enum {S_CD, S_RM, S_REGRESS, S_HELP, S_MAN, S_UMASK};

void
printShellUsage()
{
    printf("Below are the commands currently available from the basic shell\n");
    printf("  breakpoint - enter the user-level debugger\n");
    printf("  cat - limited, is a builtin until have support to pass cwd\n");
    printf("  cd - change directory\n");
    printf("  echo - print whatever follows echo\n");
    printf("  exit - leave the shell and return back to kernel test loop\n");
    printf("  help - print this\n");
    printf("  history - print the last n commands\n");
    printf("  ls - print contents of a directory\n");
    printf("  logout - leave the shell and return back to kernel test loop\n");
    printf("  regress - run the regression-script from "
				"/tests/native/regression-script\n");
    printf("  man - print this\n");
    printf("  rm - remove a file\n");
    printf("  trace_print - prints out tracing info\n");
    printf("  debug -[usn] - start future children with "
	       "[u]ser-level, [s]ystem-level or [n]o debugging.\n");
}


CmdRec cmdtable[] =
{
    {"echo",	echo},
    {"ls",	ls},
    {"cat",	cat},
    {"dir",	dir},
    {"history",	history},
    {"exit",	logout},
    {"logout",	logout},
    {"cd",	(SHFUNC)(uval)S_CD},   // Shell object builtins below
    {"rm",	(SHFUNC)(uval)S_RM},
    {"regress",	(SHFUNC)(uval)S_REGRESS},
    {"help",	(SHFUNC)(uval)S_HELP},
    {"man",	(SHFUNC)(uval)S_MAN},
    {"breakpoint", breakcmd},
    {"marc", marccmd},
    {"debug", debug},
    {"trace_print", trace_print},
    {"login", login},
    {"0",        0}
};

CmdRec*
cmdsearch(char *cmd)
{
    CmdRec *cmdptr = cmdtable;

    while (strcmp(cmdptr->name, "0") != 0) {
	if (!strcmp(cmd, cmdptr->name))
	   return cmdptr;
	++cmdptr;
    }
    return 0;
}

void
Shell::cd(char *lpath)
{
    SysStatus rc;
    if (lpath == NULL) {
	lpath = "/";
    }
    err_printf("shell doing \"cd\" to %s\n", lpath);
    rc = FileLinux::Chdir(lpath);
    if (_FAILURE(rc)) {
	err_printf("%s: Not a directory\n", lpath);
    }
}

void
Shell::rm(char */*lpath*/)
{
    tassertWrn(0, "rm not yet implemented\n");
}

void
Shell::help(char * /*lpath*/)
{
    printShellUsage();
}

void
Shell::man(char * /*lpath*/)
{
    printShellUsage();
}

void
Shell::executeBuiltin(uval cmd, uval /* argc */, char *argv[])
{
    switch (cmd) {
    case S_CD:
	cd(argv[1]);
	break;
    case S_RM:
	rm(argv[1]);
	break;
    case S_REGRESS:
	regress(argv[1]);
	break;
    case S_HELP:
	help(argv[1]);
	break;
    case S_MAN:
	man(argv[1]);
	break;
    default:
	printf("invalid shell builtin %ld\n", cmd);
	break;
    }
}


SysStatus
Shell::doBuiltInCmds(uval argc, char *argv[])
{
    CmdRec *builtin;

    if ((builtin = cmdsearch(argv[0]))) {
	if ((uval)builtin->func <= S_UMASK) { // Check for class built-ins.
	    executeBuiltin((uval)builtin->func, argc, argv);
	} else {
	    (*(builtin->func))(argc, argv);
	}
	return 0;
    }
    return 1;
}

uval
echo(uval /* argc */, char *argv[])
{
    printf("%s\n", argv[1]);
    return 0;
}

uval
cat(uval argc, char *argv[])
{
    FileLinuxRef flr;
    SysStatus rc;

    if (argc < 2 || argv[1] == NULL) {
	    err_printf("cat: nothing to do\n");
	    return 1;
    }

    err_printf("doing cat on file %s\n", argv[1]);

    // FIXME: put in mode IO::FORREAD...
    rc = FileLinux::Create(flr, argv[1], O_RDONLY, 0);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed "
		       "_SERROR =(%lu,%lu,%lu)\n",
		        argv[1], _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	return (rc);
    }
    char *p=0;
    uval lenp;
    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(flr)->readAlloc(4096, p, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    if (_FAILURE(rc) && _SCLSCD(rc) == FileLinux::EndOfFile) {
	// empty file
	return 0;
    }
    tassert(_SUCCESS(rc), err_printf("woops\n"));
    lenp = _SGETUVAL(rc);

    GenState avail;
    DREF(tty)->write(p,lenp, NULL, avail);

    return 0;
}

void
Shell::regress(char *lpath)
{
    SysStatus rc;
    FileLinuxRef flr;
    char regressFileName[32];

    err_printf("going to run regression test from "
		    "/tests/native/regression-script\n");
    strcpy(regressFileName, "/tests/native/regression-script");

    rc = FileLinux::Create(flr, regressFileName, O_RDONLY, 0);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed\n", regressFileName);
	return;
    }
    char *p=0;
    char command[128];
    char echo[128];
    uval lenp;

    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(flr)->readAlloc(4096, p, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    tassert(_SUCCESS(rc), err_printf("woops\n"));
    lenp = _SGETUVAL(rc);

    // this is a bad hack but we don't have fgets
    uval i,j,n;
    n=0;
    while (1) {
	i=0;
	while (i<sizeof(command) && p[n]!= '#' && n<lenp) {
	    command[i++] = p[n++];
	}
	if (i==sizeof(command)) {
	    command[sizeof(command)-1] = 0;
	    printf("parse error in %s after \n%s\n",
		       regressFileName, command);
	    break;
	}
	command[i] = '\0';
	n++;
	j=n;
	i=0;
	while (i<sizeof(echo) && p[n]!= 0xa && n<lenp) {
	    echo[i++] = p[n++];
	}
	if (i==sizeof(echo)) {
	    command[sizeof(echo)-1] = 0;
	    printf("parse error in %s after \n%s\n",
		       regressFileName, echo);
	    break;
	}
	echo[i] = '\0';
	if (command[0] && command[0] != '*') {
	    printf("going to run command: %s\n%s\n\n", command, echo);
	    rc = processCmdLine(command);
	    printf("\n");
	    if (rc == 0) {
		printf("command ran successfully\n");
	    } else {
		printf("command failed with rc %ld\n", rc);
	    }
	    printf("\n");
	}
	while (p[n] == 0xa&& n<lenp) n++;
	if (p[n]==0 || n>=lenp) break;
    }

    DREF(flr)->readFree(p);
    DREF(flr)->destroy();
    DREFGOBJ(TheProcessRef)->regress();

    printf("regression test finished\n");
    return;
}

uval
dir(uval argc , char *argv[])
{
    return ls(argc, argv);
}

uval
history(uval /* argc */, char *argv[])
{
    uval i;

    argv[0][0] = '\0';  // touch argv to avoid warning
    if (shell.cmdCount < MAX_HISTORY_LEN) {
	for (i=0; i<shell.cmdCount; i++) {
	    printf("\t%ld\t%s\n",i,shell.hist[i]);
	}
    }
    else {
	for (i=0; i<MAX_HISTORY_LEN; i++) {
	    printf("\t%ld\t%s\n",shell.cmdCount-(MAX_HISTORY_LEN - i),
		       shell.hist[(i+shell.cmdCount)%MAX_HISTORY_LEN]);
	}
    }
    return 0;
}

uval
logout(uval /* argc */, char *argv[])
{
    argv[0][0] = '\0';  // touch argv to avoid warning
    shell.runningShell = 0;
    return 0;
}

uval
ls(uval argc, char *argv[])
{
    SysStatus rc;
    char *dname;
    FileLinuxRef flr;

    if (argc > 1) {
	dname = argv[1];
    } else {
	dname = "";
    }
    rc = FileLinux::Create(flr, dname, O_RDONLY, 0);
    if (_FAILURE(rc)) {
	err_printf("open of directory \"%s\" failed _SERROR =(%ld,%ld,%ld)\n",
		   dname, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	return rc;
    }

    uval i = 1;
    printf("\n");
    for (;;) {
	SysStatusUval rv;
	//Max to comfortably fit in a PPC page
	struct direntk42 dbuf[(PPCPAGE_LENGTH_MAX/sizeof(struct direntk42))];

	rv = DREF(flr)->getDents(dbuf, sizeof(dbuf));
	if (_FAILURE(rv)) {
	    if (((i-1)%4)!=0) printf("\n");
	    err_printf("read of directory %s failed "
		       "_SERROR =(%lu,%lu,%lu)\n",
		       dname, _SERRCD(rv), _SCLSCD(rv), _SGENCD(rv));
	    DREF(flr)->destroy();
	    return rv;
	} else if (_SGETUVAL(rv) == 0) {
	    if (((i-1)%4)!=0) printf("\n");
	    DREF(flr)->destroy();
	    return 0;
	}

	struct direntk42 *dp = dbuf;
	while (rv > 0) {
	    printf("%-18s", dp->d_name);
	    if ((i++ % 4) == 0) printf("\n");
	    rv -= dp->d_reclen;
	    dp = (struct direntk42 *)((uval)dp + dp->d_reclen);
	}
    }
}

uval
breakcmd(uval argc, char *argv[])
{
    if (argc>1) {
	DREFGOBJ(TheProcessRef)->breakpoint();
    } else {
	breakpoint();
	// asm("tw 31,0,0");
    }
    return 0;
}

uval
trace_print(uval argc, char *argv[])
{
    SysStatus rc;

    printf("in trace print here\n");
    rc = DREFGOBJ(TheSystemMiscRef)->tracePrintBuffers();

    return 0;
}

uval
debug(uval argc, char *argv[])
{
    char *msg[] = {"none", "system", "user"};
    uval dbg = 0;

    if (argc > 1) {
	switch (*argv[1]) {
	case 'u':
	    dbg = GDBIO::USER_DEBUG;
	    break;
	case 's':
	    dbg = GDBIO::DEBUG;
	    break;
	case 'n':
	    dbg = GDBIO::NO_DEBUG;
	    break;
	default:
	    dbg = !GDBIO::GetDebugMe();
	    break;
	}
    } else {
	dbg = !GDBIO::GetDebugMe();
    }

    (void) GDBIO::SetDebugMe(dbg);
    printf("debug set to %s\n", msg[dbg]);

    return 0;
}

#include <stub/StubLogin.H>
uval
login(uval argc, char *argv[])
{
    SysStatus rc = 0;
//FIXME: DEAD CODE, this can't work anymore
#if 0

    SysStatusProcessID initPID;
    ObjectHandle oh;

    tassertMsg(0, "DEAD CODE\n");
    initPID = StubLogin::_GetLoginPID();
    tassert(_SUCCESS(initPID), err_printf("login: GetPID failed\n"));

    rc = DREF(tty)->giveAccessByClient(oh, _SGETPID(initPID));
    tassert(_SUCCESS(rc), err_printf("login: giveAccess failed\n"));

    char term[256];
    rc = StubKBootParms::_GetParameterValue("TERM", term, 256);
    if (_FAILURE(rc) || term[0] == '\0') {
	err_printf("login: TERM unknown, using dumb\n");
	strcpy(term, "dumb");
    }

    tassertMsg(0, "DEAD CODE\n");
    if (argc > 1 && argv[1] != NULL) {
	rc = StubLogin::LoginOH(oh, argv[1], term);
    } else {
	rc = StubLogin::LoginOH(oh, "k42", term);
    }
    if (_SUCCESS(rc)) {
	return 0;
    }

    err_printf("login: Login failed\n");
#endif
    return rc;
}

sval atoi(char* p)
{
    sval i=0;
    sval s=1;
    char c;
    while ((c=*p++)) {
	if (c=='-') s=s*(-1);
	if (c==' ') continue;
	if (c<'0'||c>'9') break;
	i=i*10+(c-'0');
    }
    return s*i;
}

#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>

void
forkCheck(uval a1, uval a2, uval page, uval check)
{
    passert(*(uval*)(a1+page*PAGE_SIZE)==check,
	    err_printf("forkCheck1 %lx %lx\n",a1,page));
    if (a2) {
	passert(*(uval*)(a2+page*PAGE_SIZE)==check,
	    err_printf("forkCheck2 %lx %lx\n",a2,page));
    }
    // if two pages verify they are different
    if (a1 && a2) {
	*(uval*)(a1+page*PAGE_SIZE+8)=1;
	passert(*(uval*)(a2+page*PAGE_SIZE+8)==0,
		err_printf("forkCheck3 %lx %lx\n",a2,page));
    }
}

#include <sys/ProcessLinux.H>
#include <usr/ProgExec.H>
#include <sys/ioctl.h>

uval
marccmd(uval argc, char *argv[])
{
    SysStatus rc;

//    	(void) setsid();
    rc = DREFGOBJ(TheProcessLinuxRef)->becomeInit();
    DREFGOBJ(TheProcessLinuxRef)->setsid();
//    if (ioctl(1, TIOCSCTTY, (char *)NULL) == -1)
    rc = DREF(tty)->ioctl(TIOCSCTTY, 0);
    if (_FAILURE(rc)) err_printf("oops %lx\n", rc);
    return 0;
#if 0
    sval status;
    pid_t waitfor, childLinuxPID, parent_pid;
    ProcessLinux::LinuxInfo linuxInfo;
    rc = DREFGOBJ(TheProcessLinuxRef)->becomeInit();
    if (_FAILURE(rc)) {
	err_printf("already init %lx\n", rc);
    }


    parent_pid = _SGETUVAL(DREFGOBJ(TheProcessRef)->getPID());

    rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	ProcessLinux::SETRESGID, 377, 377, 377, 377, 100, 100, 100, 100);

    if (_FAILURE(rc)) {
	err_printf("oops SETRESGID %lx\n", rc);
    }
    rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	ProcessLinux::SETFSGID, 377, 377, 377, 377, 100, 100, 100, 100);

    if (_FAILURE(rc)) {
	err_printf("oops SETFSGID %lx\n", rc);
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	ProcessLinux::SETRESUID, 377, 377, 377, 377, 100, 100, 100, 100);
    if (_FAILURE(rc)) {
	err_printf("oops SETRESUID %lx\n", rc);
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	ProcessLinux::SETFSUID,  377, 377, 377, 377, 100, 100, 100, 100);
    if (_FAILURE(rc)) {
	err_printf("oops SETFSUID %lx\n", rc);
    }

    return 0;
    rc = ProgExec::ForkProcess(childLinuxPID);
    if (_FAILURE(rc)) {
	err_printf("oops2 %lx\n", rc);
	return rc;
    }
    if (childLinuxPID == 0) {
	ProcessLinux::creds_t *lcp;
	//change something so we can tell the difference
	rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	    ProcessLinux::SETFSUID,  0, 0, 1, 5, 0, 3, 3, 4);
	if (_FAILURE(rc)) {
	    err_printf("oops SETFSUID %lx\n", rc);
	}

	rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
	err_printf("child %lx creds %ld %ld %ld %ld %ld %ld %ld %ld \n",
		   uval(linuxInfo.pid),
		   uval(linuxInfo.creds.uid),
		   uval(linuxInfo.creds.euid),
		   uval(linuxInfo.creds.suid),
		   uval(linuxInfo.creds.fsuid),
		   uval(linuxInfo.creds.gid),
		   uval(linuxInfo.creds.egid),
		   uval(linuxInfo.creds.sgid),
		   uval(linuxInfo.creds.fsgid));
	rc = DREFGOBJ(TheProcessLinuxRef)->
	    getCredsPointerNativePid(parent_pid, lcp);
	if (_FAILURE(rc)) err_printf("getCredsPointer rc %lx\n", rc);
	else {
	err_printf("child parent creds %ld %ld %ld %ld %ld %ld %ld %ld \n",
		   uval(lcp->uid),
		   uval(lcp->euid),
		   uval(lcp->suid),
		   uval(lcp->fsuid),
		   uval(lcp->gid),
		   uval(lcp->egid),
		   uval(lcp->sgid),
		   uval(lcp->fsgid));
	}
	rc = DREFGOBJ(TheProcessLinuxRef)->
	    releaseCredsPointer(lcp);
	DREFGOBJ(TheProcessLinuxRef)->exit(5);
	// NOTREACHED
	tassertMsg(0, "Returned from exit()!\n");
    }
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    err_printf("parent %ld %ld %ld %ld %ld %ld %ld %ld \n",
	       uval(linuxInfo.creds.uid),
	       uval(linuxInfo.creds.euid),
	       uval(linuxInfo.creds.suid),
	       uval(linuxInfo.creds.fsuid),
	       uval(linuxInfo.creds.gid),
	       uval(linuxInfo.creds.egid),
	       uval(linuxInfo.creds.sgid),
	       uval(linuxInfo.creds.fsgid));

    err_printf("parent waiting for %lx\n", sval(childLinuxPID));
    waitfor = -1;
    rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(waitfor,status,0);
    if (_FAILURE(rc)) {
	err_printf("oops3 %lx\n", rc);
	return rc;
    }
    err_printf("pid %lx status %lx\n", sval(waitfor), status);
    return 0;

    uval paddr, caddr, ccaddr, size;
    SysStatus rc;
    ObjectHandle frOH, childfrOH;
    size = 8*PAGE_SIZE;

    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	paddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0);
    passert(_SUCCESS(rc), err_printf("woops2\n"));

    Obj::ReleaseAccess(frOH);

    // initialize pages 0, 1 , 4 - leave two uninitalized

    *(uval*)(paddr) = 0xf0;
    *(uval*)(paddr+PAGE_SIZE) = 0xf1;
    *(uval*)(paddr+4*PAGE_SIZE) = 0xf4;
    *(uval*)(paddr+6*PAGE_SIZE) = 0xf6;


    // fork copy
    rc = DREFGOBJ(TheProcessRef)->forkCopy(paddr, childfrOH);
    passert(_SUCCESS(rc), err_printf("woops3\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	caddr, size, 0, childfrOH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0);
    passert(_SUCCESS(rc), err_printf("woops4\n"));

    Obj::ReleaseAccess(childfrOH);

    Obj::ReleaseAccess(childfrOH);

    // page zero first on parent
    forkCheck(paddr, caddr, 0, 0xf0);
    // page one first on child
    forkCheck(caddr, paddr, 1, 0xf1);
    // page two first on parent
    forkCheck(paddr, caddr, 2, 0x0);
    // page three first on child
    forkCheck(caddr, paddr, 3, 0x0);
    // now check second child
    forkCheck(paddr, ccaddr, 6, 0xf6);
    forkCheck(ccaddr, paddr, 7, 0);

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(paddr);
    passert(_SUCCESS(rc), err_printf("woops5\n"));

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(caddr);
    passert(_SUCCESS(rc), err_printf("woops5a\n"));

    forkCheck(ccaddr, 0, 4, 0xf4);
    forkCheck(ccaddr, 0, 5, 0);

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(ccaddr);
    passert(_SUCCESS(rc), err_printf("woops6\n"));

    return 0;
#if 0

    __asm (
	" li 0,0; "
	" mr 3,1; "
	" mr 4,2; "
	" li 5,5; "
	" li 6,6; "
	" li 7,7; "
	" li 8,8; "
	" li 9,9; "
	" li 10,10; "
	" li 11,11; "
	" li 12,12; "
	" li 13,13; "
	" li 14,14; "
	" li 15,15; "
	" li 16,16; "
	" li 17,17; "
	" li 18,18; "
	" li 19,19; "
	" li 20,20; "
	" li 21,21; "
	" li 22,22; "
	" li 23,23; "
	" li 24,24; "
	" li 25,25; "
	" li 26,26; "
	" li 27,27; "
	" li 28,28; "
	" li 29,29; "
	" li 30,30; "
	" li 31,41; "
	" mtlr 31; "
	" li 31,42; "
	" mtctr 31; "
	" li 31,43; "
	" mtcr 31; "
	" li 31,44; "
	" mtxer 31; "
	" li 31,31; "
	" mfcr 31; li 1,0x100; nop; nop; nop;"
	);


    (void) argc; (void) argv;
    uval i;
    SysTime interval;
    i = atoi(argv[1]);
    interval = Scheduler::TicksPerSecond() * i;
    printf("i is %s %lx %lx %lx\n",
	   argv[1],i,uval(interval>>32),uval(interval));
    Scheduler::DelayUntil(interval, TimerEvent::relative);
    /* test cancelling an event by doing a blockwithtimeout after
     * an unblock
     */
    Scheduler::Unblock(Scheduler::GetCurThread());
    interval = Scheduler::TicksPerSecond() * 60;
    Scheduler::BlockWithTimeout(interval, TimerEvent::relative);

    return 0;
#endif
#endif
}
