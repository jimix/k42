/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: echosrv.C,v 1.19 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinuxSocket.H>
#include <io/Socket.H>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/systemAccess.H>

#define MAXLINE 512

void echoServerLoop(FileLinuxRef);

void echoServer()
{
    FileLinuxRef serverSocket;
    FileLinuxRef clientSocket;

    SocketAddrIn addrIn(0, 6000);

    cprintf("echoServer: Socket\n");

    FileLinuxSocket::Create(serverSocket, AF_INET, SOCK_STREAM, 0);

    DREF(serverSocket)->bind((char*)&addrIn, sizeof(addrIn));

    DREF(serverSocket)->listen(4);

    while (1) {
	ThreadWait *tw = NULL;
	SysStatus rc = DREF(serverSocket)->accept(clientSocket, &tw);
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

    cprintf("<--- Connection opened --->\n");

    echoServerLoop(clientSocket);

    DREF(clientSocket)->destroy();
    DREF(serverSocket)->destroy();

    cprintf("<--- Connection closed --->\n");
}

void echoServerLoop(FileLinuxRef clientSocket)
{
    sval n;
    SysStatusUval rc;
    char line[MAXLINE];

    for (;;) {
	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(clientSocket)->read(line, MAXLINE, &tw, moreAvail);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		n = (sval)rc;
		break;
	    }
	}

	if (n <= 0)
	    return;

	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(clientSocket)->write(line, n, &tw, moreAvail);
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
	line[n] = 0;
	cprintf(">> %s", line);
    }
}

void echoClientLoop(FileLinuxRef);

void echoClient()
{
    FileLinuxRef socket;

    SocketAddrIn addrIn(0, 6000);

    FileLinuxSocket::Create(socket, AF_INET, SOCK_STREAM,0);

    cprintf("echoClient1\n");
    uval addrLen = sizeof(addrIn);

    while (1) {
	ThreadWait *tw = NULL;
	GenState moreAvail;
	SysStatus rc = DREF(socket)->connect((char*)&addrIn, addrLen,
					     moreAvail, &tw);
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

    cprintf("echoClient2\n");

    echoClientLoop(socket);

    DREF(socket)->destroy();
}

void echoClientLoop(FileLinuxRef socket)
{
    int i;
    char const *msg[] = {
        "Test 1\n",
        "Test 2\n",
        "Test 3\n",
        "Test 4\n",
        "Test 5\n",
        NULL};

    for (i=0;i<5;i++) {
	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    SysStatus rc = DREF(socket)->write(msg[i], strlen(msg[i]),
					       &tw, moreAvail);
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
    }
}

int main(int argc, char *argv[])
{
    NativeProcess();

    if (atoi(argv[1]) == 0) {
	echoServer();
    } else {
	echoClient();
    }
}

