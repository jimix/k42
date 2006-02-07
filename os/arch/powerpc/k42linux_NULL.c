/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: k42linux_NULL.c,v 1.3 2005/04/20 13:39:56 butrico Exp $
 *****************************************************************************/

#define stop asm("tw 31,0,0")
/*
 * perform this on linker error to stub out the dangling references
 *   sed -ne 's/.* fit: R_PPC64_REL24 \.\(.*\)$/void \1() {stop;}/p ; s/.* undefined reference to `\([^.].*\).$/long \1 = 0xdeafbeeddeafbeed;/p' | sort -u
 */

long _ctype = 0xdeafbeeddeafbeed;
long irq_desc = 0xdeafbeeddeafbeed;
long irq_stat = 0xdeafbeeddeafbeed;
long lkCore = 0xdeafbeeddeafbeed;
long lmb = 0xdeafbeeddeafbeed;
long naca = 0xdeafbeeddeafbeed;
long openpic_vec_ipi = 0xdeafbeeddeafbeed;
long ppc_md = 0xdeafbeeddeafbeed;
long smp_num_cpus = 0xdeafbeeddeafbeed;
long tb_ticks_per_usec = 0xdeafbeeddeafbeed;



void BlockLinux_BlockRead() {stop;}
void BlockLinux_ReadVirtual() {stop;}
void BlockLinux_SimDiskValid() {stop;}
void BlockLinux_WriteVirtual() {stop;}
void LinuxBlockOpen() {stop;}
void LinuxBlockRead() {stop;}
void LinuxBlockWrite() {stop;}
void LinuxIoctl() {stop;}
void SocketLinux_SocketHardLock() {stop;}
void SocketLinux_SocketHardUnLock() {stop;}
void SocketLinux_bind() {stop;}
void SocketLinux_connect() {stop;}
void SocketLinux_destroy() {stop;}
void SocketLinux_internalCreate() {stop;}
void SocketLinux_ioctl() {stop;}
void SocketLinux_listen() {stop;}
void SocketLinux_poll() {stop;}
void SocketLinux_setsockopt() {stop;}
void SocketLinux_getsockopt() {stop;}
void SocketLinux_soAccept() {stop;}
void SocketLinux_soReceive() {stop;}
void SocketLinux_soSend() {stop;}
void __Linux_stop() {stop;}
void __start_lkAcenic() {stop;}
void __start_lkBlock() {stop;}
void __start_lkCore() {stop;}
void __start_lkNet() {stop;}
void __start_lkPCI() {stop;}
void __start_lkPCNet32() {stop;}
void _configNetDev() {stop;}
void csum_partial() {stop;}
void do_softirq() {stop;}
void kmalloc() {stop;}
void kmem_cache_reap() {stop;}
void openpic_cause_IPI() {stop;}
void openpic_set_priority() {stop;}
void request_irq() {stop;}
void shutdownArch() {stop;}
void xics_cause_IPI() {stop;}
void xics_setup_cpu() {stop;}
