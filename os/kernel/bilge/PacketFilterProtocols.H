/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilterProtocols.H,v 1.4 2002/01/10 18:05:13 peterson Exp $
 *****************************************************************************/

#ifndef __PACKET_FILTER_PROTOCOLS_H_
#define __PACKET_FILTER_PROTOCOLS_H_

#include <sys/types.H>

/* Protocol header definitions */

#define ETHER_ADDR_LEN 6

struct etherhdr {
    uval8 ether_dhost[ETHER_ADDR_LEN];
    uval8 ether_shost[ETHER_ADDR_LEN];
    uval16 ether_type;
};

struct iphdr {
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#define __BIG_ENDIAN__   // defined BIG_ENDIAN by default if neither is defined
#endif /* #if !defined(__BIG_ENDIAN__) && ... */
#if defined(__BIG_ENDIAN__)
    uval8 ip_v:4;
    uval8 ip_hl:4;
#elif defined(__LITTLE_ENDIAN__)
    uval8 ip_hl:4;
    uval8 ip_v:4;
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */
    uval8 ip_tos;
    uval16 ip_len;
    uval16 ip_id;
    uval16 ip_off;
    uval8 ip_ttl;
    uval8 ip_p;
    uval16 ip_sum;
    uval32 ip_source;
    uval32 ip_dest;
    /* The options start here */
};

struct udphdr {
    uval16 udp_source;
    uval16 udp_dest;
    uval16 udp_len;
    uval16 udp_sum;
};

struct tcphdr {
    uval16 tcp_source;
    uval16 tcp_dest;
    uval32 tcp_seq;
    uval32 tcp_ack_seq;
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#define __BIG_ENDIAN__   // defined BIG_ENDIAN by default if neither is defined
#endif /* #if !defined(__BIG_ENDIAN__) && ... */
#if defined(__BIG_ENDIAN__)
    uval16 tcp_doff:4;
    uval16 tcp_res1:4;
    uval16 tcp_res2:2;
    uval16 tcp_urg:1;
    uval16 tcp_ack:1;
    uval16 tcp_psh:1;
    uval16 tcp_rst:1;
    uval16 tcp_syn:1;
    uval16 tcp_fin:1;
#elif defined(__LITTLE_ENDIAN__)
    uval16 tcp_res1:4;
    uval16 tcp_doff:4;
    uval16 tcp_fin:1;
    uval16 tcp_syn:1;
    uval16 tcp_rst:1;
    uval16 tcp_psh:1;
    uval16 tcp_ack:1;
    uval16 tcp_urg:1;
    uval16 tcp_res2:2;
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */
    uval16 tcp_window;
    uval16 tcp_sum;
    uval16 tcp_urg_ptr;
};

#endif /* #ifndef __PACKET_FILTER_PROTOCOLS_H_ */
