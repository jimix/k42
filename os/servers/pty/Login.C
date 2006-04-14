/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Login.C,v 1.70 2005/07/15 17:14:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: login daemon.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <io/FileLinux.H>
#include <io/FileLinuxStreamTTY.H>
#include <io/FileLinuxSocket.H>
#include <scheduler/Scheduler.H>
#include "Login.H"
#include <meta/MetaLogin.H>
#include <stub/StubBaseServers.H>
#include <sys/systemAccess.H>
#include <emu/FD.H>
#include <stub/StubKBootParms.H>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include "IOForwarder.H"
#include <asm/ioctls.h>
#include "IOForwarderConsole.H"
#include <stub/StubSystemMisc.H>

/* static */ void
Login::ConsoleProcess(char* argv[], char* envp[]) {
    err_printf("baseServers is proxy to start %s\n",argv[0]);

    int tty = open("/dev/ptmx",O_RDWR|O_NOCTTY);
    tassertMsg(tty > 0, "open of /dev/ptmx returned %d\n", tty);
    unsigned int ptyNum=99;
    int ret = ioctl(tty, TIOCGPTN, &ptyNum);
    int pty_unlocked = 0;
    ret = ioctl(tty, TIOCSPTLCK, &pty_unlocked);
    FileLinuxRef ttyRef;
    FileLinuxRef dummy = NULL;

    ttyRef = _FD::GetFD(tty);
    _FD::ReplaceFD(dummy, tty);
    IOForwarderConsole* ttyIOF = new IOForwarderConsole;
    ttyIOF->init(ttyRef);

    pid_t childPid = fork();
    if (childPid==0) {
	char buf[64];
	snprintf(buf, 64, "/dev/pts/%d", ptyNum);
	Child(buf, "root", "xterm", argv);
	DREFGOBJ(TheProcessRef)->kill();
    }

    waitpid(childPid, NULL, 0);

    delete ttyIOF;
}

/* static */ int
Login::Child(const char* ptyName, const char* lname, const char* term,
	     char **argv)
{
    uval sz;

    // authenticate
    struct passwd *tpw;
    struct passwd pw;

    _FD::CloseAll();

    int stdin = open(ptyName,O_RDONLY|O_NOCTTY);
    int stdout= open(ptyName,O_WRONLY|O_NOCTTY);
    int stderr= open(ptyName,O_WRONLY|O_NOCTTY);

    tassertMsg(stdin==0 && stdout==1 && stderr==2,
	       "Bad fd's for std*: %d %d %d\n", stdin, stdout, stderr);
    // paranoia?
    memset(&pw, 0, sizeof (pw));

    tpw = getpwnam(lname);

    if (tpw == NULL) {
	printf("Login: login as %s: unknown login.\n", lname);
	printf("Login: continuing as root\n.");
	tpw = getpwnam("root");

	if (tpw == NULL) {
	    printf("Login: getpwnam(\"root\") failed: %s\n"
		   "    Creating fake entry\n", strerror(errno));
	    // make a fake one
	    tpw = (struct passwd *) alloca(sizeof (struct passwd));
	    tpw->pw_name	= "root";
	    tpw->pw_passwd	= "";
	    tpw->pw_uid		= 0;
	    tpw->pw_gid		= 0;
	    tpw->pw_gecos	= "Super User";
	    tpw->pw_dir		= "/";
	    tpw->pw_shell	= "/bin/bash";
	}

    }


    pw.pw_uid = tpw->pw_uid;
    pw.pw_gid = tpw->pw_gid;

    sz = strlen(tpw->pw_dir)+1;
    pw.pw_dir = (char *) alloca(sz);
    strncpy(pw.pw_dir, tpw->pw_dir, sz);

    sz = strlen(tpw->pw_shell)+1;
    pw.pw_shell = (char *) alloca(sz);
    strncpy(pw.pw_shell, tpw->pw_shell, sz);

    // must do this while still root
    // sets the process group ID of the current process to the current
    // process
    // setpgrp();

    // create a new session
    pid_t sid = setsid();
    tassertMsg(sid>=0, "Bad session id: %d\n", sid);

    //    if (ioctl(1, TIOCSCTTY, (char *)NULL) == -1)
    int ret = ioctl(stdin, TIOCSCTTY, 0);
    tassertMsg(ret>=0, "Login: TIOCSCTTY failed\n");
    // Set ids
    if (setregid(pw.pw_gid, pw.pw_gid) == -1) {
	printf("Login: setregid(%u) failed: %s.\n",
	       pw.pw_gid, strerror(errno));
    }
    if (setreuid(pw.pw_uid, pw.pw_uid) == -1) {
	printf("Login: setreuid(%u) failed: %s.\n",
	       pw.pw_uid, strerror(errno));
    }


    // Set PWD
    if (chdir(pw.pw_dir) == -1) {
	printf("Login: chdir(%s) failed: %s.\n"
	       "  Continuing with / as home dir\n",
	       pw.pw_dir, strerror(errno));
	// It's OK we alloca()'d it.
	pw.pw_dir = "/";
    }

    // rudimentary environment
    // HOME
    setenv("HOME", pw.pw_dir, 1);

    // SHELL
    setenv("SHELL", pw.pw_shell, 1);

    // No need for PATH... glibc takes care of it

    // PATH
    const char def_path[] = "/usr/bin:/bin:/usr/sbin:/sbin";
    setenv("PATH",def_path, 1);

    // TERM
    setenv("TERM", term, 1);

    // POSIX shells check argv[0][0] for '-' to see if it is a
    // login shell
    // basename it
    char *s;
    if (argv==NULL) {
	char *v;
	if (strlen(pw.pw_shell)!=0) {
	    s = pw.pw_shell;
	    v = strrchr(pw.pw_shell, '/') + 1;	// skip the '/'
	} else {
	    s = "/bin/sh";
	    v = "sh";
	}
	sz = strlen(v) + 1;
	argv = (char**)alloca(sizeof(char*)*2);
	argv[0] = (char *)alloca(sz + 1);
	memcpy(argv[0]+1, v, sz);
	argv[0][0] = '-';
	argv[1]=NULL;
    } else {
	s = argv[0];
    }
    printf("Starting '%s' on %s\n", s, ptyName);

    ret = execv(s, argv);

    printf("Login failed: %s: %s.\n", argv[0], strerror(errno));
    return ret;
}


static uval
ReadStr(int fd, char *buf, char del, int max=256)
{
    while (max) {
	int ret = recv(fd, buf, 1, 0);
	if (ret<0) {
	    break;
	}
	if (ret == 1) {
	    if (*buf == del) {
		*buf = 0;
		return 1;
	    }
	    ++buf;
	    --max;
	} else {
	    err_printf("DEBUG: ReadStr: read() returned %d: %s.\n",
		       errno, strerror(errno));
	}
    }
    return 0;
}

static uval
HandShake(int fd, char *lname, char *rname, char *term, char *speed)
{
    char buf[256];
    int ret;
    /*
    ** Expect:
    **	\0
    **	Name\0
    **	Name\0
    **	Term/Speed\0
    ** Response:
    **	\0
    */
    ret = recv(fd,buf,1,0);

    if (ret<0) {
	err_printf("Rlogind: read failed %d %s\n", errno, strerror(errno));
	return 0;
    }
    if (buf[0] != '\0') {
	err_printf("Rlogind: first handshake byte was not nil.\n");
	return 0;
    }
    if (!ReadStr(fd, rname, '\0', 256)<0) {
	err_printf("Rlogind: real name failure.\n");
	return 0;
    }
    if (!ReadStr(fd, lname, '\0', 256)) {
	err_printf("Rlogind: login name failure.\n");
	return 0;
    }
    if (!ReadStr(fd, term, '/', 256)) {
	err_printf("Rlogind: terminal type failure.\n");
	return 0;
    }
    if (!ReadStr(fd, speed, '\0', 256)) {
	err_printf("Rlogind: terminal speed failure.\n");
	return 0;
    }
    return 1;
}

/* static */ SysStatus
Login::__doLogin(FileLinuxRef fl, const char* uname, const char* term)
{
    SysStatus rc;

    int tty = open("/dev/ptmx",O_RDWR|O_NOCTTY);
    unsigned int ptyNum=99;
    int ret = ioctl(tty, TIOCGPTN, &ptyNum);

    int pty_unlocked = 0;
    ret = ioctl(tty, TIOCSPTLCK, &pty_unlocked);

    //Get a hold of the ref for /dev/ptmx
    FileLinuxRef ttyRef = NULL;
    _FD::ReplaceFD(ttyRef, tty);

    pid_t childPid = fork();
    if (childPid==0) {
	char buf[64];
	snprintf(buf, 64, "/dev/pts/%d", ptyNum);
	Child(buf, uname, term, NULL);
	DREFGOBJ(TheProcessRef)->kill();
    }

    IOForwarder* sockIOF = new IOForwarder;
    IOForwarder* ttyIOF = new IOForwarder;

    rc = DREF(ttyRef)->attach();
    tassertMsg(_SUCCESS(rc), "failed to attach to IO object: %lx\n",rc);

    rc = DREF(fl)->attach();
    tassertMsg(_SUCCESS(rc), "failed to attach to IO object: %lx\n",rc);

    rc = sockIOF->init(fl, ttyRef);
    tassertMsg(_SUCCESS(rc), "Init of IOForwarder failed: %lx\n",rc);
    rc = ttyIOF->init(ttyRef, fl);
    tassertMsg(_SUCCESS(rc), "Init of IOForwarder failed: %lx\n",rc);

    waitpid(childPid, NULL, 0);

    // regress depends on this
    err_printf("LoginOH: done\n");
    delete ttyIOF;
    delete sockIOF;

    return 0;
}

/* static */ void
Login::DoRlogind(uval arg)
{
    err_printf("Received incoming socket connection for login\n");

    FileLinuxRef sockRef = (FileLinuxRef)arg;
    char lname[256];
    char rname[256];
    char term[256];
    char speed[256];

    int fd = _FD::AllocFD(sockRef);
    tassertMsg(fd>=0,"Couldn't get fd: %x\n",fd);

    if (! HandShake(fd, lname, rname, term, speed)) {
	err_printf("Rlogind: HandShake failed.\n");
	return;
    }

    // FIXME
    // Technically we are not supposed to send the NULL until we have
    // been authenticated. but who wants to set the socket up to be
    // passed to the child?
    int ret = send(fd,"",1,0);
    if (ret<0) {
	close(fd);
	err_printf("Rlogind: write failed rc=%x\n", ret);
	return;
    }

    FileLinuxRef fl = NULL;
    _FD::ReplaceFD(fl, fd);

    __doLogin(sockRef, lname, term);
}


struct RLoginCB:public IONotif{
    RLoginCB():IONotif (FileLinux::READ_AVAIL) {
	flags = IONotif::Persist;
    };
    virtual void ready(FileLinuxRef fl, uval state);
};

/* virtual */ void
RLoginCB::ready(FileLinuxRef fl, uval state)
{
    SysStatus rc;
    FileLinuxRef sockRef;
    pid_t pid;

    if (state & FileLinux::READ_AVAIL) {
	rc = DREF(fl)->accept(sockRef, NULL);
	tassertMsg(_SUCCESS(rc),"Failed on rlogin accept: %lx\n", rc);

	rc = DREFGOBJ(TheProcessLinuxRef)->
		cloneNative(pid, Login::DoRlogind, (uval) sockRef);
	tassertMsg(_SUCCESS(rc), "clone failure: %lx\n", rc);

	/*
	 * FIXME:  We don't have the machinery to clean up after the clone
	 *         thread we just launched.  For now, it just becomes a zombie
	 *         when the login session terminates.  No one reaps it.
	 */
    }
}

/* static */ void
Login::ClassInit()
{
    SysStatus rc;

    MetaLogin::init();

    char envport[256];
    FileLinuxRef bindRef;
    uval port;

    /*
     * Limit the rlogin daemon to one VP, so that it can still fork() after
     * creating multiple clones.
     */
    (void) DREFGOBJ(TheProcessLinuxRef)->setVPLimit(1);

    rc = StubKBootParms::_GetParameterValue("K42_LOGIN_PORT", envport, 256);
    if (!_SUCCESS(rc) || envport[0] == '\0') {
	err_printf("Rlogind: K42_LOGIN_PORT not defined, defaulting to"
		   " tcp/login 513\n");
	port = 513;
    } else {
	port = atoi(envport);
    }

    rc = FileLinuxSocket::Create(bindRef, AF_INET, SOCK_STREAM, 0);
    if (_FAILURE(rc)) {
	err_printf("Rlogind: socket() failed: %lx\n", rc);
	//	return;
    } else {
	SocketAddrIn addrIn(0,port);

	rc = DREF(bindRef)->bind((char*)&addrIn, sizeof(addrIn));
	if (_FAILURE(rc)) {
	    err_printf("Rlogind: bind() failed for port %ld %lx.\n", port, rc);
	    return;
	}

	rc = DREF(bindRef)->notify(new RLoginCB);
	tassertMsg(_SUCCESS(rc),
		   "Failed to register notifier on listening socket: %lx\n",rc);

	rc = DREF(bindRef)->listen(4);
	tassert((_SUCCESS(rc)),
		err_printf("Rlogind: listen() failed error <%ld %ld %ld>\n",
			   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc)));

	err_printf("Rlogind waiting for first connection on port %ld.\n",
		   port);
    }

    char buf[256];
    rc = StubKBootParms::_GetParameterValue("K42_LOGIN_CONSOLE", buf, 256);
    if (!(_FAILURE(rc) || buf[0] == 0)) {
      err_printf("starting shell on console\n");
      _StartConsoleLogin();      
      err_printf("starting shell on console done\n");
    }

    err_printf("K42 ready for login\n");

    Scheduler::DeactivateSelf();
    while (1) {
	Scheduler::Block();
    }
}


/* static */ void
Login::DoRlogindConsole(uval arg)
{
    SysStatus rc;
    ProcessID pid;
    ObjectHandle oh;
    FileLinuxRef fl;

    err_printf("Received incoming console login\n");

    pid = DREFGOBJ(TheProcessRef)->getPID();
    rc = StubSystemMisc::_TakeOverConsoleForLogin(pid, oh);

    rc = FileLinuxStream::Create(fl, oh, O_RDWR);
    tassertMsg(_SUCCESS(rc), "woops\n");

//    int fd = _FD::AllocFD(fl);
//    tassertMsg(fd>=0,"Couldn't get fd: %x\n",fd);

    rc = __doLogin(fl, "root", "xterm");
}

/* static  __async */ SysStatus
Login::_StartConsoleLogin()
{
    SysStatus rc;
    pid_t pid;

    rc = DREFGOBJ(TheProcessLinuxRef)->
			cloneNative(pid, Login::DoRlogindConsole, 0);
    tassertMsg(_SUCCESS(rc), "clone failure: %lx\n", rc);

    /*
     * FIXME:  We don't have the machinery to clean up after the clone
     *         thread we just launched.  For now, it just becomes a zombie
     *         when the login session terminates.  No one reaps it.
     */
    return 0;
}

/*static*/ SysStatusProcessID
Login::_GetLoginPID()
{
    return  DREFGOBJ(TheProcessRef)->getPID();
}
