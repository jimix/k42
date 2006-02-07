/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilterCommon.H,v 1.2 2001/10/05 21:47:42 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: common filter definitions
 ****************************************************************************/

/*
 * xio_filter_common.h
 *
 * Filter header file (structures and constants)
 *
 */

#ifndef __PACKET_FILTER_COMMON_H_
#define __PACKET_FILTER_COMMON_H_

#include <sys/types.H>
#include <io/PacketRingServer.H>

#define ETHER_TYPE_IP   0x0800
#define ETHER_TYPE_ARP  0x0806

#define IP_PROTOCOL_ICMP 0x1
#define IP_PROTOCOL_TCP  0x6
#define IP_PROTOCOL_UDP  0x17

#define FILTER_TCP_SRC 0x1
#define FILTER_TCP_DST 0x2
/* TCP specific filter */
/* Valid flags: FILTER_TCP_SRC_WILD, FILTER_TCP_DST_WILD */
struct tcp_filter {
  uval8  flags;
  uval16 source_port;
  uval16 dest_port;
};

#define FILTER_UDP_SRC 0x1
#define FILTER_UDP_DST 0x2
/* UDP specific filter */
/* Valid flags: FILTER_UDP_SRC, FILTER_UDP_DST */
struct udp_filter {
  uval8  flags;
  uval16 source_port;
  uval16 dest_port;
};

#define FILTER_IP_VER  0x1
#define FILTER_IP_PROT 0x2
#define FILTER_IP_SRC  0x4
#define FILTER_IP_DST  0x8
/* IP specific filter */
/* Valid flags: FILTER_IP_VER, FILTER_IP_PROT, FILTER_IP_SRC,
 *              FILTER_IP_DST
 */
struct ip_filter {
  uval8  flags;
  uval8  version;
  uval8  protocol;
  uval32 source_addr;
  uval32 dest_addr;
  union {
    struct tcp_filter tcp_filter;
    struct udp_filter udp_filter;
  } tp;
};
#define FILTER_COPY       0x1
#define FILTER_EXCL       0x2
#define FILTER_ETHER_TYPE 0x4

/* User filter - created by app  */
/* Valid flags: one of: FILTER_COPY, FILTER_EXCL */
struct xio_user_filter {
  uval8  flags;
  uval16 ether_type;
  union {
    struct ip_filter ip_filter;
  } ether_filter;
};

struct xio_dev_filter {
  uval                    callback_arg;
  void                  (*callback)(uval, void *);
  void                   *dev_private;
  struct xio_user_filter *filter;
};

typedef struct xio_user_filter UserFilter;
typedef struct xio_dev_filter DeviceFilter;


#endif /* #ifndef __PACKET_FILTER_COMMON_H_ */

/*
 * Local variables:
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
