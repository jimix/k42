/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Socket.C,v 1.18 2001/11/01 01:09:15 mostrows Exp $
 ****************************************************************************/
#define __NO_CTYPE
#include <ctype.h>
#include <sys/sysIncs.H>
#include "Socket.H"

//FIXME: needed for htonl and htons
//FIXME: conflict in struct ip_opts between tag and member
//#include <netinet/in.h>

//FIXME: This assumes only a big endian machine
#define htonl(x) (x)
#define htons(x) (x)

uval SocketAddr::size()
{
    uval sz = 0;
    switch((uval)family) {
    case INET:
	sz =  sizeof(SocketAddrIn);
	break;
	default:
	tassertWrn(0, "unkown socket family %d\n", family);
	sz = 0;
	break;
    }
    return sz;
}

//FIXME - This assumes the family is the same for both types
SocketAddrIn::SocketAddrIn(const struct sockaddr *sockaddr_in)
{
    memcpy(this, sockaddr_in, sizeof(SocketAddrIn));
}

// Always converts to network byte order
SocketAddrIn::SocketAddrIn(const char *inAddr, uval16 inPort)
{
    family = INET;
    if(inAddr) {
	addr = InetAddr(inAddr);
    } else {
	addr = 0;
    }
    port = htons(inPort);
}

// Ascii internet address interpretation routine.
// The value returned is in network order.
/* static */ uval32
SocketAddrIn::InetAddr(const char *addr)
{
    uval32 val;
    uval32 base, n;
    uval32 parts[4];
    uval32 *pp = parts;
    char c;

    c = *addr;
    for (;;) {
	    // Collect number up to ".".
	    // Values are specified as for C:
	    // 0x=hex, 0=octal, isdigit=decimal.
	if (!isdigit(c)) {
	    return (0);
	}
	val = 0;
	base = 10;
	if (c == '0') {
	    c = *++addr;
	    if (c == 'x' || c == 'X') {
		base = 16, c = *++addr;
	    } else {
		base = 8;
	    }
	}
	for (;;) {
	    if (isascii(c) && isdigit(c)) {
		val = (val * base) + (c - '0');
		c = *++addr;
	    } else if (base == 16 && isascii(c) && isxdigit(c)) {
		val = (val << 4) |
		    (c + 10 - (islower(c) ? 'a' : 'A'));
		c = *++addr;
	    } else {
		break;
	    }
	}
	if (c == '.') {
		//Internet format:
		//a.b.c.d
		//a.b.c	(with c treated as 16 bits)
		//a.b	(with b treated as 24 bits)
	    if (pp >= parts + 3) {
		return (0);
	    }
	    *pp++ = val;
	    c = *++addr;
	} else {
	    break;
	}
    }
	// Check for trailing characters.
    if (c != '\0' && (!isascii(c) || !isspace(c))) {
	return (0);
    }
	// Concoct the address according to
	// the number of parts specified.
    n = pp - parts + 1;
    switch (n) {

	case 0:
	    return (0);		        // initial nondigit

	case 1:				// a -- 32 bits
	    break;

	case 2:				// a.b -- 8.24 bits
	    if (val > 0xffffff) {
		return (0);
	    }
	    val |= parts[0] << 24;
	    break;

	case 3:				// a.b.c -- 8.8.16 bits
	    if (val > 0xffff) {
		return (0);
	    }
	    val |= (parts[0] << 24) | (parts[1] << 16);
	    break;

	case 4:				// a.b.c.d -- 8.8.8.8 bits
	    if (val > 0xff) {
		return (0);
	    }
	    val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
	    break;
    }

    return htonl(val);
}
