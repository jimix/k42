/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cl_in_services.c,v 1.12 2004/10/27 13:56:41 mostrows Exp $
 *****************************************************************************/

/* reach into the kernel to get the BootInfo structure */
#include <sys/types.H>
#include "of_softros.h"
#include "mmu.h"
#include <stdarg.h>
#include "../../../kernel/bilge/arch/powerpc/BootInfo.H"

/* Bit encodings for Machine State Register (MSR) */
#define MSR_POW         (1<<18)         /* Enable Power Management */
#define MSR_TGPR        (1<<17)         /* TLB Update registers in use */
#define MSR_ILE         (1<<16)         /* Interrupt Little-Endian enable */
#define MSR_EE          (1<<15)         /* External Interrupt enable */
#define MSR_PR          (1<<14)         /* Supervisor/User privilege */
#define MSR_FP          (1<<13)         /* Floating Point enable */
#define MSR_ME          (1<<12)         /* Machine Check enable */
#define MSR_FE0         (1<<11)         /* Floating Exception mode 0 */
#define MSR_SE          (1<<10)         /* Single Step */
#define MSR_BE          (1<<9)          /* Branch Trace */
#define MSR_FE1         (1<<8)          /* Floating Exception mode 1 */
#define MSR_IP          (1<<6)          /* Exception prefix 0x000/0xFFF */
#define MSR_IR          (1<<5)          /* Instruction MMU enable */
#define MSR_DR          (1<<4)          /* Data MMU enable */
#define MSR_RI          (1<<1)          /* Recoverable Exception */
#define MSR_LE          (1<<0)          /* Little-Endian enable */

static __inline__ unsigned long mfmsr(void)
{
        unsigned long msr;
        __asm__ __volatile__("mfmsr %0" : "=r" (msr));
        return msr;
}


struct service_request {
	char *service;
	int n_args;
	int n_returns;
	int args[8];
};
int (*of_entry)(struct service_request *);

unsigned long call_prom(const char *service, int nargs, int nret, ...);

int
generic_of(char* call, int nIn, int nOut, int *outVals,...)
{
   	struct service_request req;
	va_list args;
	int i, retcode = -1;

	req.service = call;
	req.n_args = nIn;
	req.n_returns = nOut;

	/* copy args in */
	va_start(args, outVals);
	for (i=0; i < nIn; i++) {
		req.args[i] = va_arg(args, int);
	}
	va_end(args);

	/* call OF interface */
	retcode = of_entry(&req);

	/* copy args out */
	for (i=0; i < nOut; i++) {
		outVals[i] = req.args[i+nIn];
	}

	return retcode;
}
/*
 * This file contains functions for each of the client interface services.
 * The services defined in this file are based on the services that are
 * specified in the IEEE Standard for Boot Firmware.  Each service function
 * is structured as follows:
 *	an array of integers is declared, which is the cell argument array
 *		that is expected by the firmware provided interface handler
 *	the first element of the array is initialized to a string pointer
 *		which specifies the client interface service
 *	the second element is the number of input arguments to the interface
 *	the third element is the number of return values from the interface
 *	the fourth through the Nth elements are the input arguments
 *	the Nth+1 through the end are the return values
 */

int of_test(char *name)
{
    return call_prom("test", 1, 1, name);
}

int of_canon(char *dev, void *buf, int buflen)
{
    return call_prom("canon", 3, 1, dev, buf, buflen);
}

int of_child(int phandle)
{
    return call_prom("child", 1, 1, phandle);
}

int of_finddevice(char *dev)
{
    return call_prom("finddevice", 1, 1, dev);
}

int
of_getprop(int phandle, char *name, void *buf, int buflen)
{
    return call_prom("getprop", 4, 1, phandle, name, buf, buflen);
}

int of_getproplen(int phandle, char *name)
{
    return call_prom("getproplen", 2, 1, phandle, name);
}

int of_inst2pack(int phandle)
{
    return call_prom("instance-to-package", 1, 1, phandle);
}

int of_inst2path(int ihandle, void *buf, int buflen)
{
    return call_prom("instance-to-path", 3, 1, ihandle, buf, buflen);
}

int of_nextprop(int phandle, char *prev, void *buf)
{
    return call_prom("nextprop", 3, 1, phandle, prev, buf);
}

int of_pack2path(int phandle, void *buf, int buflen)
{
    return call_prom("package-to-path", 3, 1, phandle, buf, buflen);
}

int of_parent(int phandle)
{
    return call_prom("parent", 1, 1, phandle);
}

int of_peer(int phandle)
{
    return call_prom("peer", 1, 1, phandle);
}

int of_setprop(int phandle, char *name, void *buf, int buflen)
{
    return call_prom("setprop", 4, 1, phandle, name, buf, buflen);
}

typedef void (*prom_entry)(struct prom_args *);
unsigned int of_call_method(unsigned char arg_count, unsigned char ret_count,
	int *data_array, char *method, int ihandle)
{
    struct prom_args prom_args[16] = {{0,}};
    struct prom_args *p = &prom_args[15];
    int i = 0;
    p->service = (u32)"call-method";
    p->nargs = arg_count + 2;
    p->nret = ret_count + 1;
    p->rets = (prom_arg_t*)&p->args[p->nargs];
    p->args[0] = (int)method;
    p->args[1] = ihandle;

    while (i < arg_count) {
	p->args[2 + i] = data_array[i];
	++i;
    }

    ((prom_entry)(u32)prom.entry)(p);

    i = 0;
    while (i < ret_count) {
	data_array[i] = p->args[arg_count + 3 + i];
	++i;
    }
    return p->args[arg_count + 2];
}


/*----------------------------------------------------------------------------*/
/*		Device I/O client interface services */
/*----------------------------------------------------------------------------*/
void of_close(int ihandle)
{
    call_prom("close", 1, 0, ihandle);
}

int of_open(char *dev)
{

    return call_prom("open", 1, 1, dev);
}

int of_read(int ihandle, void *addr, int len)
{
    return call_prom("read", 3, 1, ihandle, addr, len);
}

int of_seek(int ihandle, int hipos, int lopos)
{
    return call_prom("seek", 3, 1, ihandle, hipos, lopos);
}

int of_write(int ihandle, void *addr, int len)
{
    return call_prom("write", 3, 1, addr, len);
}

/*----------------------------------------------------------------------------*/
/*		Memory client interface services */
/*----------------------------------------------------------------------------*/
int of_claim(void *virt, int size, int align)
{
    return call_prom("claim", 3, 1, virt, size, align);
}

void of_release(void *virt, int size)
{
    call_prom("release", 2, 0, virt, size);
}

/*----------------------------------------------------------------------------*/
/*		Control transfer client interface services */
/*----------------------------------------------------------------------------*/
void of_enter()
{
    call_prom("enter", 0, 0);
}

void of_exit()
{
    call_prom("exit", 0, 0);
}


unsigned int of_instantiate_rtas(unsigned int base)
{
    return call_prom("instantiate-rtas", 1, 1, base);
}

static int of_mmu = 0;
unsigned int
__of_map_range(void *phys, void *virt, int size, int mode)
{
    int cl_in_args[6];
    int i=0;
    int rets[2] = {-1,-1};
    unsigned long msr = mfmsr();
    if ( !(msr & MSR_IR) || !(msr & MSR_DR) )
	return 0;

    size = (size + PGSIZE -1) & ~(PGSIZE-1);
    if (of_mmu==0) {
	printf("Initializing OF VM calls\n\r");
	int phandle;
	phandle = of_finddevice("/chosen");
	of_getprop(phandle, "mmu", &of_mmu, 4);
    }

    if (-1 == generic_of("call-method", 6, 2, rets,
			 (int)"map", of_mmu, mode, size, virt, phys)) {
	return -1;
    }
    return rets[0];
}

unsigned int
of_translate(void *virt)
{
    unsigned int rets[4] = {-1,};
    if (-1 == generic_of("call-method", 3, 4, rets,
			(int)"translate", of_mmu,  virt)) {
	return -1;
    }
    return rets[3];
}

unsigned int
of_map_range(void *phys, void *virt, int size)
{
    __of_map_range(phys,virt,size,-1);
}


unsigned int of_start_cpu(int phandle, int pc, int r3)
{
    return call_prom("start-cpu", 3, 0, phandle, pc, r3);
}


void rtas_timebase_call(int rtas_base, int token)
{
    int rtas_in_args[6];

    rtas_in_args[0] = token;
    rtas_in_args[1] = 0;
    rtas_in_args[2] = 1;
    rtas_in_args[3] = -1;
    (*rtas_in_handler)(rtas_in_args, (void *) rtas_base);
    return;
}

void rtas_pci_call(int rtas_base, int token, int in, int out,
		   int p1, int p2, int p3, int p4)
{
    int rtas_in_args[7];

    rtas_in_args[0] = token;
    rtas_in_args[1] = in;
    rtas_in_args[2] = out;
    rtas_in_args[3] = p1;
    rtas_in_args[4] = p2;
    rtas_in_args[5] = p3;
    rtas_in_args[6] = p4;

    (*rtas_in_handler)(rtas_in_args, (void *) rtas_base);
    return;
}

int ibm_scan_log_dump_call(int rtas_base, int token, int buffer, int len)
{
    int rtas_in_args[6];

    rtas_in_args[0] = token;
    rtas_in_args[1] = 2;
    rtas_in_args[2] = 1;
    rtas_in_args[3] = buffer;
    rtas_in_args[4] = len;
    rtas_in_args[5] = -1;
    (*rtas_in_handler)(rtas_in_args, (void *) rtas_base);
    return rtas_in_args[5];
}

int event_scan_call(int rtas_base, int token,
		    int event_mask, int critical,
		    int buffer, int len)
{
    int rtas_in_args[8];

    rtas_in_args[0] = token;
    rtas_in_args[1] = 4;
    rtas_in_args[2] = 1;
    rtas_in_args[3] = event_mask;
    rtas_in_args[4] = critical;
    rtas_in_args[5] = buffer;
    rtas_in_args[6] = len;
    rtas_in_args[7] = -2;
    (*rtas_in_handler)(rtas_in_args, (void *) rtas_base);
    return rtas_in_args[7];
}


#define QUEUE_SIZE 4096
int
find_dev_type(int phandle, char *target_type)
{
    int child, phandle_queue[QUEUE_SIZE];
    char q_in, q_out;
    char prop_val[64];

    q_in = q_out = 0;
    phandle_queue[q_in++] = of_finddevice("/");

    while (q_in != q_out) {
	if ((of_getprop(phandle_queue[q_out], "device_type",
			prop_val, 64)) != -1) {
	    if (! strcmp(prop_val, target_type)) {
		if (phandle == 0)
		    return phandle_queue[q_out];
		if (phandle_queue[q_out] == phandle)
		    phandle = 0;
	    }
	}

	child = of_child(phandle_queue[q_out]);
	q_out = (q_out + 1) % QUEUE_SIZE;
	while (child) {
	    phandle_queue[q_in] = child;
	    q_in = (q_in + 1) % QUEUE_SIZE;
	    if (q_in == q_out)
		break;
	    child = of_peer(child);
	}
    }
    return 0;
}


int
of_find_node(int node, const char* prop, const char* val, int val_size)
{
    char value[256];
    int cont;
    do {
	if (!prom_next_node(&node)) return 0;
	value[0] = 0;

	call_prom("getprop", 4, 1, node, prop, value, val_size);

    } while (strncmp(value, val, val_size));
    return node;
}

