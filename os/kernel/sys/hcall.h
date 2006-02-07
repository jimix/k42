/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hcall.h,v 1.1 2004/01/29 14:35:38 jimix Exp $
 *****************************************************************************/

#ifndef _HCALL_H
#define _HCALL_H

/*
 * Hypervisor Call Function Name to Token (Table 170)
 *
 * Class is one fo the following:
 *
 *   Crit: Continuous forward progress must be made, encountering any
 *   busy resource must cause the function to be backed out and return
 *   with a "hardware busy" return code.
 *
 *   Norm: Similar to Crit, however, wait loops for slow hardware


 *   access are allowed.
 *
 */
/*	NAME			Token	  Class Mandatory 	Set	*/
#define	H_UNUSED		0x0000	/* Crit	Yes		pft	*/
#define	H_REMOVE		0x0004	/* Crit	Yes		pft	*/
#define	H_ENTER			0x0008	/* Crit	Yes		pft	*/
#define	H_READ			0x000c	/* Crit	Yes		pft	*/
#define	H_CLEAR_MOD		0x0010	/* Crit	Yes		pft	*/
#define	H_CLEAR_REF		0x0014	/* Crit	Yes		pft	*/
#define	H_PROTECT		0x0018	/* Crit	Yes		pft	*/
#define	H_GET_TCE		0x001c	/* Crit	Yes		tce	*/
#define	H_PUT_TCE		0x0020	/* Crit	Yes		tce	*/
#define	H_SET_SPRG0		0x0024	/* Crit	Yes		sprg0	*/
#define	H_SET_DABR		0x0028	/* Crit	Yes-dabr exists	dabr	*/
#define	H_PAGE_INIT		0x002c	/* Crit	Yes		copy	*/
#define	H_SET_ASR		0x0030	/* Crit	Yes-on Istar	asr	*/
#define	H_ASR_ON		0x0034	/* Crit	Yes-on Istar	asr	*/
#define	H_ASR_OFF		0x0038	/* Crit	Yes-on Istar	asr	*/
#define	H_LOGICAL_CI_LOAD	0x003c	/* Norm	Yes		debug	*/
#define	H_LOGICAL_CI_STORE	0x0040	/* Norm	Yes		debug	*/
#define	H_LOGICAL_CACHE_LOAD	0x0044	/* Crit	Yes		debug	*/
#define	H_LOGICAL_CACHE_STORE	0x0048	/* Crit	Yes		debug	*/
#define	H_LOGICAL_ICBI		0x004c	/* Norm	Yes		debug	*/
#define	H_LOGICAL_DCBF		0x0050	/* Norm	Yes		debug	*/
#define	H_GET_TERM_CHAR		0x0054	/* Crit	Yes		term	*/
#define	H_PUT_TERM_CHAR		0x0058	/* Crit	Yes		term	*/
#define	H_REAL_TO_LOGICAL	0x005c	/* Norm	Yes		perf	*/
#define	H_HYPERVISOR_DATA	0x0060	/* Norm	See below	dump	*/
					/* is mandatory if enabled by HSC
					   and is disabled by default */
#define	H_EOI			0x0064	/* Crit	Yes		int	*/
#define	H_CPPR			0x0068	/* Crit	Yes		int	*/
#define	H_IPI			0x006c	/* Crit	Yes		int	*/
#define	H_IPOLL			0x0070	/* Crit	Yes		int	*/
#define	H_XIRR			0x0074	/* Crit	Yes		int	*/
#define	H_MIGRATE_PCI_TCE	0x0078	/* Norm Yes-if LRDR	migrate */
#define H_CEDE         	        0x00e0	/* Crit Yes             splpar  */
#define H_CONFER		0x00e4
#define H_PROD			0x00e8
#define H_GET_PPP		0x00ec
#define H_SET_PPP		0x00f0
#define H_PURR			0x00f4
#define H_PIC			0x00f8
#define H_REG_CRQ		0x00fc
#define H_FREE_CRQ		0x0100
#define H_VIO_SIGNAL		0x0104
#define H_SEND_CRQ		0x0108
#define H_PUTRTCE		0x010c
#define H_COPY_RDMA		0x0110
#define H_REGISTER_LOGICAL_LAN	0x0114
#define H_FREE_LOGICAL_LAN	0x0118
#define H_ADD_LOGICAL_LAN_BUFFER 0x011c
#define H_SEND_LOGICAL_LAN	0x0120
#define H_BULK_REMOVE		0x0124
#define H_WRITE_RDMA		0x0128
#define H_READ_RDMA		0x012c
#define H_MULTICAST_CTRL	0x0130
#define H_SET_XDABR		0x0134
#define H_STUFF_TCE		0x0138
#define H_PUT_TCE_INDIRECT	0x013c
#define H_PUT_RTCE_INDERECT	0x0140
#define H_MASS_MAP_TCE		0x0144
#define H_ALRDMA		0x0148
#define H_CHANGE_LOGICAL_LAN_MAC 0x014c
#define H_VTERM_PARTNER_INFO	0x0150
#define H_REGISTER_VTERM	0x0154
#define H_FREE_VTERM		0x0158
#define H_HCA_RESV_BEGIN	0x015c
#define H_HCA_RESV_END		0x01c0
#define H_GRANT_LOGICAL		0x01c4
#define H_RESCIND_LOGICAL	0x01c8
#define H_ACCEPT_LOGICAL	0x01cc
#define H_RETURN_LOGICAL	0x01d0
#define H_FREE_LOGICAL_LAN_BUFFER 0x01d4

#define RPA_HCALL_END		0x01d4	/* set to last entry */

#define H_IS_ILLEGAL(tok) (((tok) & 0x3) != 0)

/*
 * The 0x6xxx range is allotted * to embedded hypervisor.
 */
#define VHYPE_HCALL_BASE	0x6000
#define H_EMBEDDED_BASE		0x6000
#define	H_CPU_PRIORITY		0x6000
#define	H_SLOT_DURATION		0x6004
#define	H_SLOT_CPU_PRIORITY	0x6008
#define	H_YIELD			0x600c	/* Needs to become H_CEDE. @@@ */
#define	H_CREATE_PARTITION	0x6010
#define	H_START			0x6014
#define	H_GET_TERM_BUFFER	0x6018
#define	H_PUT_TERM_BUFFER	0x601c
#define	H_SET_EXCEPTION_INFO	0x6020
#define	H_GET_SYSID		0x6024
#define	H_SET_SCHED_PARAMS	0x6028
#define H_TRANSFER_CHUNK        0x602C
#define H_MULTI_PAGE		0x6030
#define H_VM_MAP                0x6034
#define H_DESTROY_PARTITION     0x6038
#define H_CREATE_MSGQ		0x603c
#define H_SEND_ASYNC		0x6040
#define H_THREAD_CONTROL        0x6044
#define	H_RTOS			0x604c	/* Real Time OS call */

/* bp.h */
#define H_SPC_ACQUIRE		0x6050
#define H_SPC_RELEASE		0x6054
#define H_SPC_INTR_MASK		0x6058
#define H_SPC_INTR_STAT		0x605c
#define H_IIC_SPU		0x6060
#define H_IIC_PU		0x6064
#define H_MFC_DS		0x6068
#define H_MFC_MAP		0x606c
#define H_MFC_MULTI_PAGE	0x6070

/* x86 specific hcalls */
#define H_LDT			0x6074
#define H_SET_PGD		0x6078
#define H_FLUSH_TLB		0x607c
#define H_SET_TSS		0x6080
#define H_GET_PFAULT_ADDR	0x6084

/* experimental */
#define H_INSERT_IMAGE		0x6088
#define H_RTAS			0x608c

/*
 * Hidden
 */
#define H_GET_XIVE		0x614c  /*PHYP # 0x504c */
#define H_SET_XIVE		0x6150  /*PHYP # 0x5050 */
#define H_INTERRUPT		0x6154  /*PHYP # 0x5054 */

#define VHYPE_HCALL_END		H_INTERRUPT

/*
 * Flags argument to H_SET_EXCEPTION_INFO.
 */
#define H_Set_SRR		0x01
#define H_Set_vec		0x02


/* Yield Flags */
#define H_SELF_SYSID		-1


/* RTOS Flags */
#define H_RTOS_REQUEST (1)



/*
 * MSR State on entrance to Hypervisor (Table 171?)
 */
#ifdef __ASSEMBLER__
#define ULL(x) x
#else
#define ULL(x) ((uval64)(x))
#endif

/*
 * The Hcall() Flags Field Definition (Table 172)
 */
#define	H_NUMA_CEC	(~(ULL(1)<<(63-15+1)-1))	/* bits 0-15	*/

#define	H_Blank_1	(ULL(1)<<(63-17))

#define	H_Exact		(ULL(1)<<(63-24))
#define	H_R_XLATE	(ULL(1)<<(63-25))
#define	H_READ_4	(ULL(1)<<(63-26))

#define	H_AVPN		(ULL(1)<<(63-32))
#define	H_andcond	(ULL(1)<<(63-33))

#define	H_I_Cache_Inv	(ULL(1)<<(63-40))
#define	H_I_Cache_Sync	(ULL(1)<<(63-41))
#define	H_Blank_2	(ULL(1)<<(63-42))

#define	H_Zero_Page	(ULL(1)<<(63-48))
#define	H_Copy_Page	(ULL(1)<<(63-49))
#define	H_Blank_3	(ULL(1)<<(63-50))

#define	H_N		(ULL(1)<<(63-61))
#define	H_pp1		(ULL(1)<<(63-62))
#define	H_pp2		(ULL(1)<<(63-63))

#define H_VM_MAP_ICACHE_INVALIDATE      (ULL(1)<<(63-40))
#define H_VM_MAP_ICACHE_SYNCRONIZE      (ULL(1)<<(63-41))
#define H_VM_MAP_INVALIDATE_TRANSLATION (ULL(1)<<(63-42))
#define H_VM_MAP_INSERT_TRANSLATION     (ULL(1)<<(63-43))
#define H_VM_MAP_LARGE_PAGE             (ULL(1)<<(63-44))
#define H_VM_MAP_ZERO_PAGE              (ULL(1)<<(63-48))

/* thread control flags */
#define H_THREAD_CONTROL_START   0x1
#define H_THREAD_CONTROL_STOP    0x2
#define H_THREAD_CONTROL_QUERY   0x4

/*
 * Hypervisor Call Return Codes (Table 173)
 */
#define	H_PARTIAL_STORE	16
#define	H_PAGE_REGISTERED 15
#define	H_IN_PROGRESS	14
#define	H_Sensor_CH	13	/* Sensor value >= Critical high	*/
#define	H_Sensor_WH	12	/* Sensor value >= Warning high		*/
#define	H_Sensor_Norm	11	/* Sensor value normal			*/
#define	H_Sensor_WL	10	/* Sensor value <= Warning low		*/
#define	H_Sensor_CL	 9	/* Sensor value <= Critical low		*/
#define	H_Partial	 5
#define	H_Constrained	 4
#define	H_Closed	 2	/* virtual terminal session is closed	*/
#define	H_Busy		 1	/* Hardware Busy -- Retry Later		*/
#define	H_Success	 0
#define	H_Hardware	-1	/* Error 				*/
#define	H_Function	-2	/* Not Supported			*/
#define	H_Privilege	-3	/* Caller not in privileged mode	*/
#define	H_Parameter	-4	/* Outside Valid Range for Partition
				   or conflicting			*/
#define H_Bad_Mode	-5	/* Illegal MSR value			*/
#define	H_PTEG_FULL	-6	/* The requested pteg was full		*/
#define	H_NOT_FOUND	-7	/* The requested pte was not found	*/
#define	H_RESERVED_DABR	-8	/* The requested address is reserved
				   by the Hypervisor on this
				   processor				*/
#define H_UNAVAIL	-9	/* Requested resource unavailable */
#define H_INVAL		-10	/* Requested parameter is invalid */
#define H_Permission	-11
#define H_Dropped	-12
#define H_S_Parm	-13
#define H_D_Parm	-14
#define H_R_Parm	-15
#define H_Resource	-16
#define H_ADAPTER_PARM	-17

#define H_IS_FUNTION_IN_PROGRESS(rc) ((rc) >= 0x0100000 && (rc) <= 0x0fffffff)

/*
 * compatibility With Linux Labels, perhpas we should ifdef this linux
 * and/or kernel.
 */
#define	H_Not_Found		H_NOT_FOUND
#define	H_ANDCOND		H_andcond
#define	H_LARGE_PAGE		H_Large_Page
#define	H_ICACHE_INVALIDATE	H_I_Cache_Inv
#define	H_ICACHE_SYNCHRONIZE	H_I_Cache_Sync
#define	H_ZERO_PAGE		H_Zero_Page
#define H_COPY_PAGE		H_Copy_Page
#define	H_EXACT			H_Exact
#define	H_PTEG_Full 		H_PTEG_FULL
#define	H_PP1			H_pp1
#define	H_PP2			H_pp2

#ifdef  __ASSEMBLER__

#define HSC .long 0x44000022

#else

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

extern sval hcall_create_partition(uval *retvals, uval chunk,
				   uval base, uval size, uval offset);
extern sval hcall_destroy_partition(uval *retvals, uval SysID);

extern sval hcall_insert_image(uval *retvals, int sys_id, uval logical_dst,
			       uval logical_src, uval size);
extern sval hcall_transfer_chunk(uval *retvals, uval chunkd, uval SysID);
extern sval hcall_start(uval *retvals, uval SysID, uval pc,
			uval r2, uval r3, uval r4,
			uval r5, uval r6, uval r7);

extern sval hcall_yield(uval *retvals, uval condition);
extern sval hcall_rtos(uval *retvals, uval flags, uval time);
extern sval hcall_set_exception_info(uval *retvals, uval cpu, uval flags,
				     uval srrloc, uval exvec);
extern sval hcall_enter(uval *retvals, uval flags, uval index, ...);
extern sval hcall_vm_map(uval *retvals, uval flags, uval pvpn, uval ptel);
extern sval hcall_read(uval *retvals, uval flags, uval index);
extern sval hcall_remove(uval *retvals, uval flags, uval pte_index, uval avpn);
extern sval hcall_clear_mod(uval *retvals, uval flags, uval pte_index);
extern sval hcall_clear_ref(uval *retvals, uval flags, uval pte_index);
extern sval hcall_protect(uval *retvals, uval flags,
			  uval pte_index, uval avpn);
extern sval hcall_get_SysID(uval *retvals);
extern sval hcall_set_sched_params(uval *retvals, uval SysID,
				   uval cpu, uval required, uval desired);
extern sval hcall_yield_check_regs(uval mySysID, uval regsave[]);
extern sval hcall_put_term_buffer(uval *retvals, uval channel,
				  uval length, uval buffer);
extern sval hcall_get_term_char(uval *retvals, uval index);
extern sval hcall_put_term_char(uval *retvals, uval index,
				uval count, ...);
extern sval hcall_thread_control(uval *retvals, uval flags,
				 uval thread_num, uval start_addr);
extern sval hcall_cede(uval *retvals);
extern sval hcall_page_init(uval *retvals, uval flags,
			    uval destination, uval source);
extern sval hcall_set_asr(uval *retvals, uval value);  /* ISTAR only. */
extern sval hcall_asr_on(uval *retvals);  /* ISTAR only. */
extern sval hcall_asr_off(uval *retvals);  /* ISTAR only. */
extern sval hcall_eoi(uval *retvals, uval xirr);
extern sval hcall_cppr(uval *retvals, uval cppr);
extern sval hcall_ipi(uval *retvals, uval sn, uval mfrr);
extern sval hcall_ipoll(uval *retvals, uval sn);
extern sval hcall_xirr(uval *retvals);
extern sval hcall_logical_ci_load_64(uval *retvals, uval size,
				     uval addrAndVal);
extern sval hcall_logical_ci_store_64(uval *retvals, uval size,
				      uval addr, uval value);
extern sval hcall_logical_cache_load_64(uval *retvals, uval size,
					uval addrAndVal);
extern sval hcall_logical_cache_store_64(uval *retvals, uval size, 
					 uval addr, uval value);
extern sval hcall_logical_icbi(uval *retvals, uval addr);
extern sval hcall_logical_dcbf(uval *retvals, uval addr);
extern sval hcall_set_dabr(uval *retvals, uval dabr);
extern sval hcall_hypervisor_data(uval *retvals, sval64 control);
extern sval hcall_create_msgq(uval *retvals, uval lbase,
			      uval size, uval vector);
extern sval hcall_send_async(uval *retvals, uval dest,
			     uval arg1, uval arg2, uval arg3, uval arg4);
extern sval hcall_real_to_logical(uval *retvals, uval raddr);


/* Hidden */
extern sval hcall_set_xive(uval *retvals, uval32 intr,
			   uval32 serv, uval32 prio);
extern sval hcall_get_xive(uval *retvals, uval32 intr);
extern sval hcall_interrupt(uval *retvals, uval32 intr, uval32 set);


static inline uval
hcall_put_term_string(int channel, int count, const char *str)
{
	uval ret = 0;
	int i;
	union {
		uval64 oct[2];
		uval32 quad[4];
		char c[16];
	} ch;

	for (i = 0; i < count; i++) {
		int m = i % sizeof(ch);
		ch.c[m] = str[i];
		if ((m == (sizeof(ch) - 1)) || 
		    (i == (count - 1))      || 
		    (str[i] == (char) 0)) 
		  {
			ret = hcall_put_term_char(0, channel, m + 1,
						  ch.oct[0], ch.oct[1]
				);
			if (ret != H_Success) {
				return ret;
			}
			if (str[i] == (char) 0)
			  {
			    return H_Success;
			  }
		}
	}
	return ret;
}

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* !__ASSEMBLER__ */


#endif /* ! _HCALL_H */
