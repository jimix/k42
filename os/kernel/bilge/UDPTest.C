/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UDPTest.C,v 1.5 2001/11/01 19:54:08 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple test of UDP services.
 * **************************************************************************/

#include "kernIncs.H"
#include <io/FileLinuxPacket.H>
#include <io/Socket.H>

#define MAX_BUF_SIZE  4096

void UDPRecv(int size, int port)
{
    int loop;
    char buf[MAX_BUF_SIZE];
    FileLinuxRef socket;

    SocketAddrIn addrIn(0, 6000);
    SocketAddrIn addrFrom;

    FileLinuxPacket::Create(socket);

    DREF(socket)->bind(addrIn, sizeof(addrIn));

    loop=0;
    for(;;) {
	DREF(socket)->recvfrom(buf, size, addrInFrom, sizeof(addrInFrom));

//     if(loop%100 == 0)
	err_printf("loop %d\n", loop);

	loop++;
    }

    DREF(socket)->destroy();
}

void UDPSend(int size, char *ip_addr, int port)
{
    int i, j;
    int n;
    int count = 0;
    char buf[MAX_BUF_SIZE];
    FileLinuxRef socket;
    SocketAddrIn addrIn(0, 6000);

    FileLinuxPacket::Create(socket);

    for(i=0;i<1000;i++) {
      for(j=0;j<size;j++)
        buf[j] = i;

      for(j=0;j<size;j++)
        count += buf[j];

      n = DREF(socket)->sendto(buf, size, addrIn, sizeof(addrIn));
    }
}

void UDPTest(int type)
{
  if(type == 0) {
    UDPRecv(1024, 6000);
  } else {
    UDPSend(1024, "192.168.1.2", 6000);
  }
}
