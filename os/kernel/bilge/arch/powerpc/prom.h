/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: prom.h,v 1.1 2004/02/27 17:14:28 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: temporary copy of file until ihandle is fixed in linux
 * **************************************************************************/

#ifndef _PPC64_PROM_H
#define _PPC64_PROM_H

/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh computers.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 * Updates for PPC64 by Peter Bergner & David Engebretsen, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define PTRRELOC(x)     (x) /*k42((typeof(x))((u64)(x) - offset))*/
#define PTRUNRELOC(x)   (x) /*k42((typeof(x))((u64)(x) + offset))*/
#define RELOC(x)        (x) /*k42(*PTRRELOC(&(x)))*/

#define LONG_LSW(X) (((u64)X) & 0xffffffff)
#define LONG_MSW(X) (((u64)X) >> 32)
#ifdef BOOT_PROGRAM
#define fill32 int:32
#else
#define fill32
#endif

typedef u32 phandle;
typedef u32 ihandle;
typedef u32 phandle32;
typedef u32 ihandle32;

extern char *prom_display_paths[];
extern u32 prom_num_displays;

struct address_range {
	u64 space;
	u64 address;
	u64 size;
};

struct interrupt_info {
	int	line;
	int	sense;		/* +ve/-ve logic, edge or level, etc. */
};

struct pci_address {
	u32 a_hi;
	u32 a_mid;
	u32 a_lo;
};

struct pci_range32 {
	struct pci_address child_addr;
	u32  parent_addr;
  	u64 size;
};

struct pci_range64 {
	struct pci_address child_addr;
  	u64 parent_addr;
        u64 size;
};

union pci_range {
	struct {
		struct pci_address addr;
		u32 phys;
		u32 size_hi;
	} pci32;
	struct {
		struct pci_address addr;
		u32 phys_hi;
		u32 phys_lo;
		u32 size_hi;
		u32 size_lo;
	} pci64;
};

struct _of_tce_table {
	phandle node;
	u64 base;
	u64 size;
};

struct reg_property {
	u64 address;
	u64 size;
};

struct reg_property32 {
	u32 address;
	u32 size;
};

struct reg_property64 {
	u64 address;
	u64 size;
};

struct reg_property_pmac {
        u32 address_hi;
        u32 address_lo;
        u32 size;
};


struct translation_property {
	u64 virt;
	u64 size;
	u64 phys;
	u32 flags;
};

struct property {
	fill32;
	char	*name;
	int	length;
	fill32;
	fill32;
	unsigned char *value;
	fill32;
	struct property *next;
};

/* NOTE: the device_node contains PCI specific info for pci devices.
 * This perhaps could be hung off the device_node with another struct,
 * but for now it is directly in the node.  The phb ptr is a good
 * indication of a real PCI node.  Other nodes leave these fields zeroed.
 */
struct pci_controller;
struct TceTable;
struct device_node {
	fill32;
	char	*name;
	fill32;
	char	*type;
	phandle  node;
	phandle  linux_phandle;
	int	n_addrs;
	fill32;
	fill32;
	struct	address_range *addrs;
	int	n_intrs;
	fill32;
	fill32;
	struct	interrupt_info *intrs;
	fill32;
	char	*full_name;

	/* PCI stuff probably doesn't belong here */
	int	busno;			/* for pci devices */
	int	bussubno;		/* for pci devices */
	int	devfn;			/* for pci devices */
#define DN_STATUS_BIST_FAILED (1<<0)
	int	status;			/* Current device status (non-zero is bad) */
	int	eeh_mode;		/* See eeh.h for possible EEH_MODEs */
	int	eeh_config_addr;
	fill32;
	struct  pci_controller *phb;	/* for pci devices */
	fill32;
	struct	iommu_table *iommu_table;	/* for phb's or bridges */

	fill32;
	struct	property *properties;
	fill32;
	struct	device_node *parent;
	fill32;
	struct	device_node *child;
	fill32;
	struct	device_node *sibling;
	fill32;
	struct	device_node *next;	/* next device of same type */
	fill32;
	struct	device_node *allnext;	/* next in list of all nodes */
	fill32;
	struct  proc_dir_entry *pde;       /* this node's proc directory */
	fill32;
	struct  proc_dir_entry *name_link; /* name symlink */
	fill32;
	struct  proc_dir_entry *addr_link; /* addr symlink */
	uval64	_users;                 /* reference count */
	uval64	_flags;
};

typedef u32 prom_arg_t;

struct prom_args {
        u32 service;
        u32 nargs;
        u32 nret;
        prom_arg_t args[10];
	fill32;
	fill32;
        prom_arg_t *rets;     /* Pointer to return values in args[16]. */
};

struct prom_t {
	u64 entry;
	ihandle root;
	ihandle chosen;
	int cpu;
	ihandle stdout;
	ihandle disp_node;
	struct prom_args args;
	u64 version;
	u64 encode_phys_size;
	fill32;
	struct bi_record *bi_recs;
};

extern struct prom_t prom;

extern int boot_cpuid;
#if 0

/* Prototypes */
extern void abort(void);
extern u64 prom_init(u64, u64, u64, u64, u64);
extern void prom_print(const char *msg);
extern void relocate_nodes(void);
extern void finish_device_tree(void);
extern struct device_node *find_devices(const char *name);
extern struct device_node *find_type_devices(const char *type);
extern struct device_node *find_path_device(const char *path);
extern struct device_node *find_compatible_devices(const char *type,
						   const char *compat);
extern struct device_node *find_all_nodes(void);
extern int device_is_compatible(struct device_node *device, const char *);
extern int machine_is_compatible(const char *compat);
extern unsigned char *get_property(struct device_node *node, const char *name,
				   int *lenp);
extern void print_properties(struct device_node *node);
extern int prom_n_addr_cells(struct device_node* np);
extern int prom_n_size_cells(struct device_node* np);
extern void prom_get_irq_senses(unsigned char *senses, int off, int max);
extern void prom_add_property(struct device_node* np, struct property* prop);
#endif
#endif /* _PPC64_PROM_H */
