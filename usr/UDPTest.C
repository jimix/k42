/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UDPTest.C,v 1.18 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinuxSocket.H>
#include <io/Socket.H>
#include <io/FileLinuxPacket.H>
#include <stdio.h>
#include <stdlib.h>
#include <sys/systemAccess.H>

#define MAX_BUF_SIZE  4096

void UDPRecv(int size, int port)
{
   int loop;
   char buf[MAX_BUF_SIZE];
   FileLinuxRef socket;
   SocketAddrIn addrIn(0, 6000);
   SocketAddrIn addrInFrom(0,0);

   FileLinuxPacket::Create(socket, AF_INET, SOCK_DGRAM, 0);

   DREF(socket)->bind((char*)&addrIn, sizeof(addrIn));

   loop=0;
   for (;;)
   {
       while (1) {
	   GenState moreAvail;
	   ThreadWait *tw = NULL;
	   uval socklen = sizeof(addrIn);
	   SysStatus rc = DREF(socket)->recvfrom(buf, size, 0,
						 (char*)&addrInFrom,
						 socklen, &tw, moreAvail);
	   if (_FAILURE(rc) && tw) {
	       while (!tw->unBlocked()) {
		   Scheduler::Block();
	       }
	       tw->destroy();
	       delete tw;
	   } else {
	       break;
	   }
       }

     if (loop%100 == 0)
       err_printf("loop %d\n", loop);

     loop++;
   }

   DREF(socket)->destroy();
}

void UDPSend(int size, char *ip_addr, int port)
{
    int i, j;
    SysStatusUval rc;
    int count = 0;
    char buf[MAX_BUF_SIZE];
    FileLinuxRef socket;
    SocketAddrIn addrIn(0, 6000);

    FileLinuxPacket::Create(socket, AF_INET, SOCK_DGRAM, 0);

    for (i=0;i<1000;i++)
    {
	for (j=0;j<size;j++)
	    buf[j] = i;

	for (j=0;j<size;j++)
	    count += buf[j];

	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(socket)->sendto(buf, size, 0, (char*)&addrIn,
				      sizeof(addrIn), &tw, moreAvail);
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
	UDPRecv(1024, 6000);
    } else {
	UDPSend(1024, "9.2.133.34", 6000);
    }
}
