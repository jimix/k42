/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: prom.c,v 1.12 2004/10/27 13:56:42 mostrows Exp $
 *****************************************************************************/
/* Based on Linux file arch/ppc/kernel/prom.c*/

#include <stdarg.h>
#include <sys/types.H>

#define DEBUG_PROM

#define NULL ((void*)0)
#define MAX_PROPERTY_LENGTH	4096
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define IC_INVALID    0
#define IC_OPEN_PIC   1
#define IC_PPC_XIC    2
#define LinuxNaca naca_struct
/* reach into the kernel to get the BootInfo structure */
#include "../../../kernel/bilge/arch/powerpc/BootInfo.H"
#include "lmb.h"

#define reloc_offset() 0

typedef void (*prom_entry)(struct prom_args *);
struct prom_t prom;

uval min(uval a, uval b)
{
    if (a<b) return a;
    return b;
}

static unsigned long prom_initialize_lmb(unsigned long mem);

static char *
strstr(const char *haystack, const char *needle)
{
	while (haystack[0]) {
		const char *x = haystack;
		const char *y = needle;
		while (x[0] && y[0] && x[0]==y[0]) {
			++x;
			++y;
		}
		if (!y[0]) {
			return (char*)haystack;
		}
		++haystack;
	}
	return NULL;
}

void *
memset(void *s, int c, unsigned int n)
{
	while (n>0) {
		((char*)s)[--n] = (char)c;
	}
	return s;
}

void*
memcpy(void* t, const void* s, unsigned n)
{
	while (n>0) {
		((char*)t)[n-1] = ((char*)s)[n-1];
		--n;
	}
	return t;
}


/* Return the bit position of the most significant 1 bit in a word */
static int __ilog2(unsigned int x)
{
	int l = ~0;
	while (x) {
		x= x>>1;
		++l;
	}
	return l;
}

//#define LONG_LSW(X) (((unsigned long)X) & 0xffffffff)
//#define LONG_MSW(X) (((unsigned long)X) >> 32)


extern char *prom_display_paths[];
extern unsigned int prom_num_displays;

struct isa_reg_property {
	u32 space;
	u32 address;
	u32 size;
};

/*
 * 32 bit filling necessary to make this struct look right in 64-bit space.
 */
struct TceTable;


#define MAX_PHB 16 * 3  /* 16 Towers * 3 PHBs/tower*/
struct _of_tce_table *of_tce_table=NULL;



struct LinuxNaca *naca;
struct BootInfo* systemcfg;
struct prom_args;

static u32 encode_phys_size;
u32
call_prom(const char *service, int nargs, int nret, ...)
{
	va_list list;
	int i;
	unsigned long offset = reloc_offset();
	struct prom_args prom_args;
	prom_args.service = (u32)service;
	prom_args.nargs = nargs;
	prom_args.nret = nret;
        prom_args.rets = (prom_arg_t *)&(prom_args.args[nargs]);
	va_start(list, nret);
	for (i = 0; i < nargs; ++i) {
		prom_args.args[i] = va_arg(list, u32);
	}

	va_end(list);

	for (i = 0; i < nret; ++i)
		prom_args.rets[i] = 0;

	((prom_entry)(uval32)prom.entry)(&prom_args);

	return (u32)((nret>0) ? prom_args.rets[0] : 0);
}


extern int prom_next_node(phandle *nodep);
int
prom_next_node(phandle *nodep)
{
	phandle node;
	unsigned long offset = reloc_offset();

	if ((node = *nodep) != 0
	    && (*nodep = call_prom(RELOC("child"), 1, 1, node)) != 0)
		return 1;
	if ((*nodep = call_prom(RELOC("peer"), 1, 1, node)) != 0)
		return 1;
	for (;;) {
		if ((node = call_prom(RELOC("parent"), 1, 1, node)) == 0)
			return 0;
		if ((*nodep = call_prom(RELOC("peer"), 1, 1, node)) != 0)
			return 1;
	}
}

#define DOUBLEWORD_ALIGN(x) (((x) + sizeof(u64)-1) & -sizeof(u64))

static struct device_node *allnodes;




static void
prom_exit()
{
	struct prom_args args;
	unsigned long offset = reloc_offset();

	args.service = (u32)"exit";
	args.nargs = 0;
	args.nret = 0;
	((prom_entry)(uval32)prom.entry)(&args);
	for (;;)			/* should never get here */
		;
}



extern int onSim;
extern void mprintf(const char *buf);

void
prom_print(const char *msg)
{
	const char *p, *q;
	unsigned long offset = reloc_offset();

	if (RELOC(prom.stdout) == 0) {
	    if (onSim) mprintf(msg);
	    return;
	}

	for (p = msg; *p != 0; p = q) {
		for (q = p; *q != 0 && *q != '\n'; ++q)
			;
		if (q > p)
			call_prom(RELOC("write"), 3, 1, RELOC(prom.stdout),
				  p, q - p);
		if (*q != 0) {
			++q;
			call_prom(RELOC("write"), 3, 1, RELOC(prom.stdout),
				  RELOC("\r\n"), 2);
		}
	}
}

void
prom_panic(const char* msg)
{
    prom_print(msg);
    prom_exit();
}

static void
prom_print_hex(unsigned int v)
{
	char buf[16];
	int i, c;

	for (i = 0; i < 8; ++i) {
		c = (v >> ((7-i)*4)) & 0xf;
		c += (c >= 10)? ('a' - 10): '0';
		buf[i] = c;
	}
	buf[i] = ' ';
	buf[i+1] = 0;
	prom_print(buf);
}

void
prom_print_nl(void)
{
	unsigned long offset = reloc_offset();
	prom_print(RELOC("\n"));
}

static void PROM_BUG()
{
	prom_print("\n\nPROM_BUG\n\n");
}


static void
prom_instantiate_rtas(struct prom_t *_prom, struct rtas_t *_rtas,
		      struct systemcfg *_systemcfg);

static int prom_find_machine_type(void);

void
linux_naca_init(struct BootInfo *bootInfo)
{
	naca = &bootInfo->naca;
	memcpy(&bootInfo->prom, &prom, sizeof(prom));
	systemcfg = bootInfo;


	// We can't do this earlier, because we don't know where bootinfo is */
	/* Default machine type. */
	systemcfg->platform = prom_find_machine_type();

	memset(naca, 0, sizeof(typeof(*naca)));
	prom_initialize_lmb(0);
//	prom_instantiate_rtas(&bootInfo->prom, &bootInfo->rtas, bootInfo);

}

static void prom_init_client_services(unsigned long pp)
{
	unsigned long offset = reloc_offset();
	struct prom_t *_prom = PTRRELOC(&prom);

	/* Get a handle to the prom entry point before anything else */
	_prom->entry = pp;

	/* Init default value for phys size */
	_prom->encode_phys_size = 32;

	/* get a handle for the stdout device */
	_prom->chosen = (ihandle)call_prom(RELOC("finddevice"), 1, 1,
				       RELOC("/chosen"));
	if ((long)_prom->chosen == -1)
		prom_panic(RELOC("cannot find chosen")); /* msg won't be printed :( */

	/* get device tree root */
	_prom->root = (ihandle)call_prom(RELOC("finddevice"), 1, 1, RELOC("/"));
	if ((long)_prom->root == -1)
		prom_panic(RELOC("cannot find device tree root")); /* msg won't be printed :( */
}

static void prom_init_stdout(void)
{
	unsigned long offset = reloc_offset();
	struct prom_t *_prom = PTRRELOC(&prom);
	u32 val;

        if ((long)call_prom(RELOC("getprop"), 4, 1, _prom->chosen,
			    RELOC("stdout"), &val,
			    sizeof(val)) <= 0)
                prom_panic(RELOC("cannot find stdout"));

        _prom->stdout = (ihandle)(unsigned long)val;
}

static int prom_find_machine_type(void)
{
	unsigned long offset = reloc_offset();
	struct prom_t *_prom = PTRRELOC(&prom);
	char compat[256];
	int len, i = 0;

	len = (int)(long)call_prom(RELOC("getprop"), 4, 1, _prom->root,
				   RELOC("compatible"),
				   compat, sizeof(compat)-1);
	if (len > 0) {
		compat[len] = 0;
		while (i < len) {
			char *p = &compat[i];
			int sl = strlen(p);
			if (sl == 0)
				break;
			if (strstr(p, RELOC("Power Macintosh")) ||
			    strstr(p, RELOC("MacRISC4")))
				return PLATFORM_POWERMAC;
			i += sl + 1;
		}
	}
	/* Default to pSeries */
	return PLATFORM_PSERIES;
}

void
linux_prom_init(unsigned long r3, unsigned long r4, unsigned long pp,
		unsigned long r6, unsigned long r7)
{
	struct prom_t *_prom = &prom;
        u32 getprop_rval;
	ihandle prom_cpu;
	phandle cpu_pkg;
	ihandle prom_root;
	char *p;
	extern void* of_entry;
        struct systemcfg *_systemcfg = RELOC(systemcfg);
	memset(_prom, 0, sizeof(struct prom_t));

	/* Init interface to Open Firmware and pickup bi-recs */
	prom_init_client_services(pp);

	/* Init prom stdout device */
	prom_init_stdout();


#if 0
	/* check out if we have bi_recs */
	_prom->bi_recs = prom_bi_rec_verify((struct bi_record *)r6);
	if ( _prom->bi_recs != NULL )
		RELOC(klimit) = PTRUNRELOC((unsigned long)_prom->bi_recs +
					   _prom->bi_recs->data[1]);
	/* Default machine type. */
	_systemcfg->platform = prom_find_machine_type();

	/* Get the full OF pathname of the stdout device */
	p = (char *) mem;
	memset(p, 0, 256);
	call_prom(RELOC("instance-to-path"), 3, 1, _prom->stdout, p, 255);
	RELOC(of_stdout_device) = PTRUNRELOC(p);
	mem += strlen(p) + 1;
#endif

	getprop_rval = 1;
	call_prom(RELOC("getprop"), 4, 1,
		  _prom->root, RELOC("#size-cells"),
		  &getprop_rval, sizeof(getprop_rval));
	_prom->encode_phys_size = (getprop_rval == 1) ? 32 : 64;

	/* Determine which cpu is actually running right _now_ */
        if ((long)call_prom(RELOC("getprop"), 4, 1, _prom->chosen,
			    RELOC("cpu"), &getprop_rval,
			    sizeof(getprop_rval)) <= 0)
                prom_exit();

	prom_cpu = (ihandle)(unsigned long)getprop_rval;
	cpu_pkg = call_prom(RELOC("instance-to-package"), 1, 1, prom_cpu);
	call_prom(RELOC("getprop"), 4, 1,
		cpu_pkg, RELOC("reg"),
		&getprop_rval, sizeof(getprop_rval));
	_prom->cpu = (int)(unsigned long)getprop_rval;

}



static unsigned long
inspect_node(phandle node, struct device_node *dad,
	     unsigned long mem_start, unsigned long mem_end,
	     struct device_node ***allnextpp)
{
	int l;
	phandle child;
	struct device_node *np;
	struct property *pp, **prev_propp;
	char *prev_name, *namep;
	unsigned char *valp;
	unsigned long offset = reloc_offset();

	np = (struct device_node *) mem_start;
	mem_start += sizeof(struct device_node);
	memset(np, 0, sizeof(*np));
	np->node = node;
	**allnextpp = PTRUNRELOC(np);
	*allnextpp = &np->allnext;
	if (dad != 0) {
		np->parent = PTRUNRELOC(dad);
		/* we temporarily use the `next' field as `last_child'. */
		if (dad->next == 0)
			dad->child = PTRUNRELOC(np);
		else
			dad->next->sibling = PTRUNRELOC(np);
		dad->next = np;
	}

	/* get and store all properties */
	prev_propp = &np->properties;
	prev_name = RELOC("");
	for (;;) {
		pp = (struct property *) mem_start;
		namep = (char *) (pp + 1);
		pp->name = PTRUNRELOC(namep);

		if ((long) call_prom(RELOC("nextprop"), 3, 1, node, prev_name,
				    namep) <= 0)
			break;
		mem_start = DOUBLEWORD_ALIGN((unsigned long)namep + strlen(namep) + 1);
		prev_name = namep;
		valp = (unsigned char *) mem_start;
		pp->value = PTRUNRELOC(valp);
		pp->length = (int)
			call_prom(RELOC("getprop"), 4, 1, node, namep,
				  valp, mem_end - mem_start);
		if (pp->length < 0)
			continue;
		mem_start = DOUBLEWORD_ALIGN(mem_start + pp->length);
		*prev_propp = PTRUNRELOC(pp);
		prev_propp = &pp->next;
	}
	/* Add a "linux_phandle" value */
        if (np->node != 0) {
		u32 ibm_phandle = 0;
		int len;

                /* First see if "ibm,phandle" exists and use its value */
                len = (int)
                        call_prom(RELOC("getprop"), 4, 1, node, RELOC("ibm,phandle"),
                                  &ibm_phandle, sizeof(ibm_phandle));
                if (len < 0) {
                        np->linux_phandle = np->node;
                } else {
                        np->linux_phandle = ibm_phandle;
		}
	}

	*prev_propp = 0;

	/* get the node's full name */
	l = (int) call_prom(RELOC("package-to-path"), 3, 1, node,
			    (char *) mem_start, mem_end - mem_start);
	if (l >= 0) {
		np->full_name = PTRUNRELOC((char *) mem_start);
		*(char *)(mem_start + l) = 0;
		mem_start = DOUBLEWORD_ALIGN(mem_start + l + 1);
	}

	/* do all our children */
	child = call_prom(RELOC("child"), 1, 1, node);
	while (child != 0) {
		mem_start = inspect_node(child, np, mem_start, mem_end,
					 allnextpp);
		child = call_prom(RELOC("peer"), 1, 1, child);
	}

	return mem_start;
}



/*
 * Make a copy of the device tree from the PROM.
 */
unsigned long
copy_device_tree(unsigned long mem_start, unsigned long mem_end)
{
	phandle root;
	unsigned long new_start;
	struct device_node **allnextp;
	unsigned long offset = reloc_offset();

	root = call_prom(RELOC("peer"), 1, 1, (phandle)0);
	if (root == (phandle)0) {
		prom_print(RELOC("couldn't get device tree root\n"));
		prom_exit();
	}
	allnextp = &RELOC(allnodes);
	mem_start = DOUBLEWORD_ALIGN(mem_start);
	new_start = inspect_node(root, 0, mem_start, mem_end, &allnextp);
	*allnextp = 0;
	return new_start;
}


void
prom_dump_lmb(void)
{
        unsigned long i;
        unsigned long offset = reloc_offset();
	struct lmb *_lmb  = PTRRELOC(&lmb);

        prom_print(RELOC("\nprom_dump_lmb:\n"));
        prom_print(RELOC("    memory.cnt                  = 0x"));
        prom_print_hex(_lmb->memory.cnt);
	prom_print_nl();
        prom_print(RELOC("    memory.size                 = 0x"));
        prom_print_hex(_lmb->memory.size);
	prom_print_nl();
        for (i=0; i < _lmb->memory.cnt ;i++) {
                prom_print(RELOC("    memory.region[0x"));
		prom_print_hex(i);
		prom_print(RELOC("].base       = 0x"));
                prom_print_hex(_lmb->memory.region[i].base);
		prom_print_nl();
                prom_print(RELOC("                      .physbase = 0x"));
                prom_print_hex(_lmb->memory.region[i].physbase);
		prom_print_nl();
                prom_print(RELOC("                      .size     = 0x"));
                prom_print_hex(_lmb->memory.region[i].size);
		prom_print_nl();
        }

	prom_print_nl();
        prom_print(RELOC("    reserved.cnt                  = 0x"));
        prom_print_hex(_lmb->reserved.cnt);
	prom_print_nl();
        prom_print(RELOC("    reserved.size                 = 0x"));
        prom_print_hex(_lmb->reserved.size);
	prom_print_nl();
        for (i=0; i < _lmb->reserved.cnt ;i++) {
                prom_print(RELOC("    reserved.region[0x"));
		prom_print_hex(i);
		prom_print(RELOC("].base       = 0x"));
                prom_print_hex(_lmb->reserved.region[i].base);
		prom_print_nl();
                prom_print(RELOC("                      .physbase = 0x"));
                prom_print_hex(_lmb->reserved.region[i].physbase);
		prom_print_nl();
                prom_print(RELOC("                      .size     = 0x"));
                prom_print_hex(_lmb->reserved.region[i].size);
		prom_print_nl();
        }
}


static unsigned long
prom_initialize_lmb(unsigned long mem)
{
	phandle node;
	char type[64];
        unsigned long long i, offset = reloc_offset();
	struct prom_t *_prom = PTRRELOC(&prom);
	union lmb_reg_property reg;
	unsigned long long mem_size, lmb_base, lmb_size;
	unsigned long num_regs, bytes_per_reg = (_prom->encode_phys_size*2)/8;
        struct systemcfg *_systemcfg = RELOC(systemcfg);

	lmb_init();

	/* XXX Quick HACK. Proper fix is to drop those structures and
         * properly use #address-cells. PowerMac has #size-cell set to
         * 1 and #address-cells t o 2
         */
        if (_systemcfg->platform == PLATFORM_POWERMAC)
                bytes_per_reg = 12;

        for (node = 0; prom_next_node(&node); ) {
                type[0] = 0;
                call_prom(RELOC("getprop"), 4, 1, node, RELOC("device_type"),
                          type, sizeof(type));
                if (strcmp(type, RELOC("memory")))
			continue;

		num_regs = call_prom(RELOC("getprop"), 4, 1, node, RELOC("reg"),
			&reg, sizeof(reg)) / bytes_per_reg;

		for (i=0; i < num_regs ;i++) {
			if (_systemcfg->platform == PLATFORM_POWERMAC) {
                                lmb_base = ((u64)reg.addrPM[i].address_hi) << 32;
                                lmb_base |= (u64)reg.addrPM[i].address_lo;
                                lmb_size = reg.addrPM[i].size;
                                if (lmb_base > 0x80000000ull) {
					prom_print(RELOC("Skipping memory above 2Gb for now, not yet supported\n\r"));
                                        continue;
                                }
                        } else if (_prom->encode_phys_size == 32) {
				lmb_base = reg.addr32[i].address;
				lmb_size = reg.addr32[i].size;
			} else {
				lmb_base = reg.addr64[i].address;
				lmb_size = reg.addr64[i].size;
			}
			printf("lmb addr range: %016llX[%016llX]\n\r",
			       lmb_base, lmb_size);

			if ( lmb_add(lmb_base, lmb_size) < 0 )
				prom_print(RELOC("Too many LMB's, discarding this one...\n"));
		}

	}

	lmb_analyze();

#ifdef DEBUG_PROM
	prom_dump_lmb();
#endif /* DEBUG_PROM */
	return mem;
}

void
prom_initialize_tce_table(void)
{
	phandle node;
	ihandle phb_node;
	u64 tmp;
        unsigned long offset = reloc_offset();
	char compatible[64], path[64], type[64], model[64];
	unsigned long i, table = 0;
	unsigned long long base, vbase, align;
	unsigned int minalign, minsize;
	struct _of_tce_table *prom_tce_table = RELOC(of_tce_table);
	unsigned long long tce_entry, *tce_entryp;
#ifdef DEBUG_PROM
	prom_print(RELOC("starting prom_initialize_tce_table\n"));
#endif

	/* Search all nodes looking for PHBs. */
	for (node = 0; prom_next_node(&node); ) {
		compatible[0] = 0;
		type[0] = 0;
		call_prom(RELOC("getprop"), 4, 1, node, RELOC("compatible"),
			  compatible, sizeof(compatible));
		call_prom(RELOC("getprop"), 4, 1, node, RELOC("device_type"),
			  type, sizeof(type));
		call_prom(RELOC("getprop"), 4, 1, node, RELOC("model"),
			  model, sizeof(model));

		/* Keep the old logic in tack to avoid regression. */
		if (compatible[0] != 0) {
			if ((strstr(compatible, RELOC("python")) == NULL) &&
			   (strstr(compatible, RELOC("Speedwagon")) == NULL) &&
			   (strstr(compatible, RELOC("Winnipeg")) == NULL))
				continue;
		} else if (model[0] != 0) {
			if ((strstr(model, RELOC("ython")) == NULL) &&
			    (strstr(model, RELOC("peedwagon")) == NULL) &&
			    (strstr(model, RELOC("innipeg")) == NULL))
				continue;
		}

		if ((type[0] == 0) || (strstr(type, RELOC("pci")) == NULL)) {
			continue;
		}

		if (call_prom(RELOC("getprop"), 4, 1, node,
			     RELOC("tce-table-minalign"), &minalign,
			     sizeof(minalign)) < 0) {
			minalign = 0;
		}

		if (call_prom(RELOC("getprop"), 4, 1, node,
			     RELOC("tce-table-minsize"), &minsize,
			     sizeof(minsize)) < 0) {
			minsize = 4UL << 20;
		}

		/* Even though we read what OF wants, we just set the table
		 * size to 4 MB.  This is enough to map 2GB of PCI DMA space.
		 * By doing this, we avoid the pitfalls of trying to DMA to
		 * MMIO space and the DMA alias hole.
		 */
		/*
		 * On POWER4, firmware sets the TCE region by assuming
		 * each TCE table is 8MB. Using this memory for anything
		 * else will impact performance, so we always allocate 8MB.
		 * Anton
		 *
		 * XXX FIXME use a cpu feature here
		 */
		minsize = 8UL << 20;

		/* Align to the greater of the align or size */
		align = (minalign < minsize) ? minsize : minalign;

		/* Carve out storage for the TCE table. */
		base = lmb_alloc(minsize, align);

		if ( !base ) {
			prom_print(RELOC("ERROR, cannot find space for TCE table.\n"));
			prom_exit();
		}

/*k42		vbase = absolute_to_virt(base);*/
		vbase = base;

		/* Save away the TCE table attributes for later use. */
		prom_tce_table[table].node = node;
		prom_tce_table[table].base = vbase;
		prom_tce_table[table].size = minsize;

#ifdef DEBUG_PROM
		prom_print(RELOC("TCE table: 0x"));
		prom_print_hex(table);
		prom_print_nl();

		prom_print(RELOC("\tnode = 0x"));
		prom_print_hex(node);
		prom_print_nl();

		prom_print(RELOC("\tbase = 0x"));
		prom_print_hex(vbase);
		prom_print_nl();

		prom_print(RELOC("\tsize = 0x"));
		prom_print_hex(minsize);
		prom_print_nl();
#endif
		/* Initialize the table to have a one-to-one mapping
		 * over the allocated size.
		 */
		tce_entryp = (unsigned long long*)(long)base;
		for (i = 0; i < (minsize >> 3) ;tce_entryp++, i++) {
			tce_entry = (i << PAGE_SHIFT);
			tce_entry |= 0x3;
			*tce_entryp = tce_entry;
		}

		/* Call OF to setup the TCE hardware */
		if (call_prom(RELOC("package-to-path"), 3, 1, node,
                             path, 255) <= 0) {
                        prom_print(RELOC("package-to-path failed\n"));
                } else {
                        prom_print(RELOC("opened "));
                        prom_print(path);
                        prom_print_nl();
                }

                phb_node = (ihandle)call_prom(RELOC("open"), 1, 1, path);

                if ( ((long)phb_node) <= 0) {
                        prom_print(RELOC("open failed\n"));
                } else {
                        prom_print(RELOC("open success\n"));
                }
                call_prom(RELOC("call-method"), 6, 0,
                             RELOC("set-64-bit-addressing"),
			     (u32)phb_node,
			     -1,
                             minsize,
			     (u32)(base & 0xffffffff),
                             (u32)((base >> 32ULL) & 0xffffffff));
                call_prom(RELOC("close"), 1, 0, (u32)phb_node);

		table++;
	}

	/* Flag the first invalid entry */
	prom_tce_table[table].node = 0;
#ifdef DEBUG_PROM
	prom_print(RELOC("ending prom_initialize_tce_table\n"));
#endif
}

/* Save TCE data */
unsigned long
copy_TCE_data(unsigned long mem_start, unsigned long mem_end)
{
	of_tce_table = (struct _of_tce_table*)mem_start;

	/* This is the size linux expects */
	mem_start += sizeof(struct _of_tce_table) * (MAX_PHB + 1);

	prom_initialize_tce_table();
	return mem_start;
}

#define AREA_OFTREE	0
#define AREA_TCE_DATA	1

unsigned long
linuxSaveOFData(unsigned long mem_start, unsigned long mem_end)
{
	/* There are two (or potentially more) distinct data areas
	 * that need to be copied into the kernel; a copy of the
	 * OF device tree, and the TCE configuration. Use a
	 * null-terminated array of offsets to identify these
	 */
	int num_areas = 2;
	u64* ptr = (u64*)mem_start;
	ptr[num_areas] = 0;

	mem_start += (num_areas+1)* 8;

	ptr[AREA_OFTREE]=mem_start;
	prom_print("OF tree: ");
	prom_print_hex(ptr[AREA_OFTREE]);
	prom_print_nl();
	mem_start = copy_device_tree(mem_start, mem_end);

	if (systemcfg->platform == PLATFORM_PSERIES && !onSim) {
	    ptr[AREA_TCE_DATA] = mem_start;
	    prom_print("TCE: ");
	    prom_print_hex(ptr[AREA_TCE_DATA]);
	    prom_print_nl();
	    mem_start = copy_TCE_data(mem_start, mem_end);
	} else {
	    ptr[AREA_TCE_DATA] = 0;
	}

	return mem_start;
}


unsigned long
prom_initialize_naca(unsigned long mem)
{
	phandle node;
	char type[64];
        unsigned long num_cpus = 0;
        unsigned long offset = reloc_offset();
	struct prom_t *_prom = PTRRELOC(&prom);
        struct naca_struct *_naca = RELOC(naca);
        struct systemcfg *_systemcfg = RELOC(systemcfg);

#ifdef DEBUG_PROM
	prom_print(RELOC("prom_initialize_naca: start...\n"));
#endif

	_naca->pftSize = 0;	/* ilog2 of htab size.  computed below. */

        for (node = 0; prom_next_node(&node); ) {
                type[0] = 0;
                call_prom(RELOC("getprop"), 4, 1, node, RELOC("device_type"),
                          type, sizeof(type));

                if (!strcmp(type, RELOC("cpu"))) {
			num_cpus += 1;

			/* We're assuming *all* of the CPUs have the same
			 * d-cache and i-cache sizes... -Peter
			 */
			if ( num_cpus == 1 ) {
				u32 size, lsize;

				call_prom(RELOC("getprop"), 4, 1, node,
					  RELOC("d-cache-size"),
					  &size, sizeof(size));

				if (_systemcfg->platform == PLATFORM_POWERMAC)
					call_prom(RELOC("getprop"), 4, 1, node,
						  RELOC("d-cache-block-size"),
						  &lsize, sizeof(lsize));
                                else
					call_prom(RELOC("getprop"), 4, 1, node,
						  RELOC("d-cache-line-size"),
						  &lsize, sizeof(lsize));

				_systemcfg->dCacheL1Size = size;
				_systemcfg->dCacheL1LineSize = lsize;
				_naca->dCacheL1LogLineSize  = __ilog2(lsize);
				_naca->dCacheL1LinesPerPage = PAGE_SIZE/lsize;

				call_prom(RELOC("getprop"), 4, 1, node,
					  RELOC("i-cache-size"),
					  &size, sizeof(size));

				if (_systemcfg->platform == PLATFORM_POWERMAC)
					call_prom(RELOC("getprop"), 4, 1, node,
                                                  RELOC("i-cache-block-size"),
                                                  &lsize, sizeof(lsize));
                                else
					call_prom(RELOC("getprop"), 4, 1, node,
						  RELOC("i-cache-line-size"),
						  &lsize, sizeof(lsize));

				_systemcfg->iCacheL1Size = size;
				_systemcfg->iCacheL1LineSize = lsize;
				_naca->iCacheL1LogLineSize  = __ilog2(lsize);
				_naca->iCacheL1LinesPerPage = PAGE_SIZE/lsize;

				if (_systemcfg->platform == PLATFORM_PSERIES_LPAR) {
					u32 pft_size[2];
					call_prom(RELOC("getprop"), 4, 1, node,
						  RELOC("ibm,pft-size"),
						  &pft_size, sizeof(pft_size));
				/* pft_size[0] is the NUMA CEC cookie */
					_naca->pftSize = pft_size[1];
				}
			}
                } else if (!strcmp(type, RELOC("serial"))) {
			phandle isa, pci;
			struct isa_reg_property reg;
			union pci_range ranges;

			if (_systemcfg->platform == PLATFORM_POWERMAC) {
			    phandle parent;
			    struct reg_property32 reg[16];
			    if (strcmp(type, RELOC("serial"))) {
				continue;
			    }


			    type[0] = 0;
			    call_prom(RELOC("getprop"), 4, 1, node,
				      RELOC("name"), type, sizeof(type));

			    if (strcmp(RELOC("ch-a"), type)) continue;

			    parent = call_prom(RELOC("parent"), 1, 1, node);
			    if (!parent) continue;

			    type[0] = 0;
			    call_prom(RELOC("getprop"), 4, 1, parent,
				      RELOC("name"), type, sizeof(type));

			    if (strcmp(type, RELOC("escc"))) {
				continue;
			    }

			    call_prom(RELOC("getprop"), 4, 1, node,
				      RELOC("reg"), reg, sizeof(reg));

			    while (parent) {
				parent = call_prom(RELOC("parent"),
						   1, 1, parent);

				if (!parent) break;
				type[0] = 0;
				call_prom(RELOC("getprop"), 4, 1, parent,
					  RELOC("name"), type, sizeof(type));

				if (!strcmp(type, RELOC("pci"))) {
				    call_prom(RELOC("getprop"), 4, 1,
					      parent, RELOC("ranges"),
					      &ranges, sizeof(ranges));
				    break;
				}
			    }

			    if ( _prom->encode_phys_size != 32 )
				PROM_BUG();

			    _naca->serialPortAddr = reg[0].address +
				ranges.pci32.phys;
			    continue;
			}

			type[0] = 0;
			call_prom(RELOC("getprop"), 4, 1, node,
				  RELOC("ibm,aix-loc"), type, sizeof(type));


			if (strcmp(type, RELOC("S1")))
				continue;

			call_prom(RELOC("getprop"), 4, 1, node, RELOC("reg"),
				  &reg, sizeof(reg));

			isa = call_prom(RELOC("parent"), 1, 1, node);
			if (!isa)
				PROM_BUG();
			pci = call_prom(RELOC("parent"), 1, 1, isa);
			if (!pci)
				PROM_BUG();

			call_prom(RELOC("getprop"), 4, 1, pci, RELOC("ranges"),
				  &ranges, sizeof(ranges));

			if ( _prom->encode_phys_size == 32 )
				_naca->serialPortAddr = ranges.pci32.phys+reg.address;
			else {
				_naca->serialPortAddr =
					((((unsigned long long)ranges.pci64.phys_hi) << 32) |
					 (ranges.pci64.phys_lo)) + reg.address;
			}
                }
	}

	_naca->interrupt_controller = IC_INVALID;
	if (_systemcfg->platform == PLATFORM_POWERMAC)
		_naca->interrupt_controller = IC_OPEN_PIC;
	else {
		_naca->interrupt_controller = IC_INVALID;
		for (node = 0; prom_next_node(&node); ) {
			type[0] = 0;
			call_prom(RELOC("getprop"), 4, 1, node, RELOC("name"),
				  type, sizeof(type));
			if (strcmp(type, RELOC("interrupt-controller"))) {
				continue;
			}
			call_prom(RELOC("getprop"), 4, 1, node, RELOC("compatible"),
				  type, sizeof(type));
			if (strstr(type, RELOC("open-pic"))) {
				_naca->interrupt_controller = IC_OPEN_PIC;
			} else if (strstr(type, RELOC("ppc-xicp"))) {
				_naca->interrupt_controller = IC_PPC_XIC;
			} else {
				prom_print(RELOC("prom: failed to recognize"
						 " interrupt-controller\n"));
				break;
			}
		}
	}

	if (_naca->interrupt_controller == IC_INVALID) {
		prom_print(RELOC("prom: failed to find interrupt-controller\n"));
		PROM_BUG();
	}

	/* We gotta have at least 1 cpu... */
        if ( (_systemcfg->processorCount = num_cpus) < 1 )
                PROM_BUG();

	_systemcfg->physicalMemorySize = lmb_phys_mem_size();

	if (_systemcfg->platform == PLATFORM_PSERIES ||
	    _systemcfg->platform == PLATFORM_POWERMAC) {
		unsigned long rnd_mem_size, pteg_count;

		/* round mem_size up to next power of 2 */
		rnd_mem_size = 1UL << __ilog2(_systemcfg->physicalMemorySize);
		if (rnd_mem_size < _systemcfg->physicalMemorySize)
			rnd_mem_size <<= 1;

		/* # pages / 2 */
		pteg_count = (rnd_mem_size >> (12 + 1));

		_naca->pftSize = __ilog2(pteg_count << 7);
	}

	if (_naca->pftSize == 0) {
		prom_print(RELOC("prom: failed to compute pftSize!\n"));
		PROM_BUG();
	}

	/*
	 * Hardcode to GP size.  I am not sure where to get this info
	 * in general, as there does not appear to be a slb-size OF
	 * entry.  At least in Condor and earlier.  DRENG
	 */
	_naca->slb_size = 64;

	/* Add an eye catcher and the systemcfg layout version number */
	strcpy(_systemcfg->eye_catcher, RELOC("SYSTEMCFG:PPC64"));
	_systemcfg->version.major = SYSTEMCFG_MAJOR;
	_systemcfg->version.minor = SYSTEMCFG_MINOR;
	_systemcfg->processor = _get_PVR();

#ifdef DEBUG_PROM
        prom_print(RELOC("systemcfg->processorCount       = 0x"));
        prom_print_hex(_systemcfg->processorCount);
        prom_print_nl();

        prom_print(RELOC("systemcfg->physicalMemorySize   = 0x"));
        prom_print_hex(_systemcfg->physicalMemorySize);
        prom_print_nl();

        prom_print(RELOC("naca->pftSize              = 0x"));
        prom_print_hex(_naca->pftSize);
        prom_print_nl();

        prom_print(RELOC("systemcfg->dCacheL1LineSize     = 0x"));
        prom_print_hex(_systemcfg->dCacheL1LineSize);
        prom_print_nl();

        prom_print(RELOC("systemcfg->iCacheL1LineSize     = 0x"));
        prom_print_hex(_systemcfg->iCacheL1LineSize);
        prom_print_nl();

        prom_print(RELOC("naca->serialPortAddr       = 0x"));
        prom_print_hex(_naca->serialPortAddr);
        prom_print_nl();

        prom_print(RELOC("naca->interrupt_controller = 0x"));
        prom_print_hex(_naca->interrupt_controller);
        prom_print_nl();

        prom_print(RELOC("systemcfg->platform             = 0x"));
        prom_print_hex(RELOC(systemcfg->platform));
        prom_print_nl();

	prom_print(RELOC("prom_initialize_naca: end...\n"));
#endif

}


//
// Assume rtas.base and size  are already set in _rtas.
static void
prom_instantiate_rtas(struct prom_t *_prom, struct rtas_t *_rtas,
		      struct systemcfg *_systemcfg)
{
	unsigned long offset = reloc_offset();
	ihandle prom_rtas;
        u32 getprop_rval;

#ifdef DEBUG_PROM
	prom_print(RELOC("prom_instantiate_rtas: start...\n"));
#endif
	prom_rtas = (ihandle)call_prom(RELOC("finddevice"), 1, 1, RELOC("/rtas"));
	if (prom_rtas != (ihandle) -1) {
		int  rc;

#if 0
		if ((rc = call_prom(RELOC("getprop"),
				  4, 1, prom_rtas,
				  RELOC("ibm,hypertas-functions"),
				  hypertas_funcs,
				  sizeof(hypertas_funcs))) > 0) {
			_systemcfg->platform = PLATFORM_PSERIES_LPAR;
		}
#endif
		call_prom(RELOC("getprop"),
			  4, 1, prom_rtas,
			  RELOC("rtas-size"),
			  &getprop_rval,
			  sizeof(getprop_rval));
	        _rtas->size = getprop_rval;
		prom_print(RELOC("instantiating rtas"));
		if (_rtas->size != 0) {
			unsigned long rtas_region = RTAS_INSTANTIATE_MAX;

			/* Grab some space within the first RTAS_INSTANTIATE_MAX bytes
			 * of physical memory (or within the RMO region) because RTAS
			 * runs in 32-bit mode and relocate off.
			 */
			if ( _systemcfg->platform == PLATFORM_PSERIES_LPAR ) {
				struct lmb *_lmb  = PTRRELOC(&lmb);
				rtas_region = min(_lmb->rmo_size, RTAS_INSTANTIATE_MAX);
			}
			_rtas->base = lmb_alloc_base(_rtas->size, PAGE_SIZE, rtas_region);

			prom_print(RELOC(" at 0x"));
			prom_print_hex(_rtas->base);

			prom_rtas = (ihandle)call_prom(RELOC("open"),
					      	1, 1, RELOC("/rtas"));
			prom_print(RELOC("..."));

			if ((long)call_prom(RELOC("call-method"), 3, 2,
						      RELOC("instantiate-rtas"),
						      prom_rtas,
						      _rtas->base) >= 0) {
				_rtas->entry = (long)_prom->args.rets[1];
			}
		}

		if (_rtas->entry <= 0) {
			prom_print(RELOC(" failed\n"));
		} else {
			prom_print(RELOC(" done\n"));
		}

#ifdef DEBUG_PROM
        	prom_print(RELOC("rtas->base                 = 0x"));
        	prom_print_hex(_rtas->base);
        	prom_print_nl();
        	prom_print(RELOC("rtas->entry                = 0x"));
        	prom_print_hex(_rtas->entry);
        	prom_print_nl();
        	prom_print(RELOC("rtas->size                 = 0x"));
        	prom_print_hex(_rtas->size);
        	prom_print_nl();
#endif
	}

#ifdef DEBUG_PROM
	prom_print(RELOC("prom_instantiate_rtas: end...\n"));
#endif
}
