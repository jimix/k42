/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hcall.S,v 1.1 2004/01/29 14:35:38 jimix Exp $
 *****************************************************************************/

#include <sys/kinclude.H>
#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>

#include <hcall.h> 

/* Code is from vhype sample code, these #defines make it compatible */
#define REG_WIDTH 8
#define STR std 
#define LDR ld
	
#define HCALL(n, name, func_code)	\
	C_TEXT_ENTRY(name); 		\
	STR r3,-REG_WIDTH(r1); 		\
	li r3,func_code; 		\
	HSC;				\
	LDR r12,-REG_WIDTH(r1);		\
	cmpi 0,r12,0;			\
        bne ret	## n;	/* only store regs if r12 != NULL */ \
	b ret0


/* Definitions for hypervisor functions... */

HCALL(1,hcall_enter, H_ENTER)
HCALL(0,hcall_vm_map, H_VM_MAP)
HCALL(8,hcall_read, H_READ)
HCALL(2,hcall_remove, H_REMOVE)
HCALL(1,hcall_clear_mod, H_CLEAR_MOD)
HCALL(1,hcall_clear_ref, H_CLEAR_REF)
HCALL(0,hcall_protect, H_PROTECT)
HCALL(2,hcall_set_exception_info, H_SET_EXCEPTION_INFO)
HCALL(0,hcall_start, H_START)
HCALL(0,hcall_yield, H_YIELD)
HCALL(1,hcall_rtos, H_RTOS)
HCALL(0,hcall_create_msgq, H_CREATE_MSGQ)
HCALL(0,hcall_send_async, H_SEND_ASYNC)
HCALL(1,hcall_create_partition, H_CREATE_PARTITION)
HCALL(0,hcall_insert_image, H_INSERT_IMAGE)
HCALL(0,hcall_destroy_partition, H_DESTROY_PARTITION)
HCALL(0,hcall_transfer_chunk, H_TRANSFER_CHUNK)
HCALL(1,hcall_get_SysID, H_GET_SYSID)
HCALL(3,hcall_set_sched_params, H_SET_SCHED_PARAMS)
HCALL(0,hcall_put_term_buffer, H_PUT_TERM_BUFFER)
HCALL(0,hcall_put_term_char, H_PUT_TERM_CHAR)
HCALL(0,hcall_thread_control, H_THREAD_CONTROL)
HCALL(0,hcall_cede, H_CEDE)
HCALL(3,hcall_get_term_char, H_GET_TERM_CHAR)

	
HCALL(0,hcall_page_init, H_PAGE_INIT)
HCALL(1,hcall_set_asr, H_SET_ASR)  /* ISTAR only. */
HCALL(0,hcall_asr_on, H_ASR_ON)  /* ISTAR only. */
HCALL(0,hcall_asr_off, H_ASR_OFF)  /* ISTAR only. */

HCALL(1,hcall_eoi, H_EOI)
HCALL(1,hcall_cppr, H_CPPR)
HCALL(2,hcall_ipi, H_IPI)
HCALL(1,hcall_ipoll, H_IPOLL)
HCALL(1,hcall_xirr, H_XIRR)

HCALL(1,hcall_logical_ci_load_64, H_LOGICAL_CI_LOAD)
HCALL(0,hcall_logical_ci_store_64, H_LOGICAL_CI_STORE)
HCALL(1,hcall_logical_cache_load_64, H_LOGICAL_CACHE_LOAD)
HCALL(0,hcall_logical_cache_store_64, H_LOGICAL_CACHE_STORE)
HCALL(0,hcall_logical_icbi, H_LOGICAL_ICBI)
HCALL(0,hcall_logical_dcbf, H_LOGICAL_DCBF)
HCALL(1,hcall_set_dabr, H_SET_DABR)
HCALL(8,hcall_hypervisor_data, H_HYPERVISOR_DATA)
HCALL(1,hcall_real_to_logical, H_REAL_TO_LOGICAL)
HCALL(0,hcall_multi_page, H_MULTI_PAGE)


HCALL(2,hcall_get_xive, H_GET_XIVE)
HCALL(0,hcall_set_xive, H_SET_XIVE)
HCALL(0,hcall_interrupt, H_INTERRUPT)


	
ret8:	STR r11,7*REG_WIDTH(r12)
ret7:	STR r10,6*REG_WIDTH(r12)
ret6:	STR r9,5*REG_WIDTH(r12)
ret5:	STR r8,4*REG_WIDTH(r12)
ret4:	STR r7,3*REG_WIDTH(r12)
ret3:	STR r6,2*REG_WIDTH(r12)
ret2:	STR r5,1*REG_WIDTH(r12)
ret1:	STR r4,0*REG_WIDTH(r12)
	nop
ret0:	blr
