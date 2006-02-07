/******************************************************************************
* K42: (C) Copyright IBM Corp. 2005.
* All Rights Reserved
*
* This file is distributed under the GNU LGPL. You should have
* received a copy of the license along with K42; see the file LICENSE.html
* in the top-level directory for more details.
*
* $Id: Loader.C,v 1.1 2005/06/07 03:46:39 jk Exp $
*****************************************************************************/

#include <sys/sysIncs.H>
#include "Loader.H"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>


struct ppc64_plt_entry
{
    void *function;
    void *r2val;
    void *unused;
};

Loader::Loader(SymbolResolver *r, void *(*f)(uval, uval &))
{
    resolver = r;
    object.buf = NULL;
    core = NULL;
    loadaddr = NULL;
    symtab = NULL;
    strtab = NULL;
    alloc = f;
}

int Loader::loadModule(char *buf, unsigned int len)
{
    uval min_addr, max_addr, align;
    unsigned int i, rc;
    Elf_Phdr *phdrs;
    Elf_Dyn *dyn;


    object.buf = buf;
    objsize = len;

    if (!sanityCheck()) {
	fprintf(stderr, "Object file failed sanity check\n");
	return -1;
    }

    phdrs = (Elf_Phdr *)(object.buf + object.ehdr->e_phoff);

    min_addr = ~0UL;
    max_addr = 0;
    align = 1;
    dyn = NULL;
    uval dyn_off = 0;

    /* first pass over program headers: retrieve required space and find dynamic
     * segment */
    for (i = 0; i < object.ehdr->e_phnum; ++i) {
	if (phdrs[i].p_type == PT_LOAD) {
	    if (min_addr > phdrs[i].p_vaddr)
		min_addr = phdrs[i].p_vaddr;
	    if (max_addr < phdrs[i].p_vaddr + phdrs[i].p_memsz)
		max_addr = phdrs[i].p_vaddr + phdrs[i].p_memsz;
	    if (phdrs[i].p_align > align)
		align = phdrs[i].p_align;
	} else if (phdrs[i].p_type == PT_DYNAMIC) {
	    dyn = (Elf_Dyn *)(object.buf + phdrs[i].p_offset);
	    dyn_off = phdrs[i].p_offset;
	}
    }

    /* Can't continue if no loadable segments or no dynamic info.  */
    if (min_addr == ~0UL || !dyn) {
	if (min_addr == ~0UL)
	    fprintf(stderr, "object has no loadable segments.\n");
	if (!dyn)
	    fprintf(stderr, "object has no dynamic segment.\n");
	return -ENOEXEC;
    }

    printf("dynamic segment at %p (offset %lx)\n", dyn, dyn_off);
    printf("address range: %lx -> %lx\n", min_addr, max_addr);
    printf("align: %ld\n", align);

    if (!(core = (char *)alloc(max_addr - min_addr, kaddr))) {
	return -ENOMEM;
    }

    loadaddr = core - min_addr;
    printf("loadaddr: %p, kaddr: 0x%016lx\n", loadaddr, kaddr);

    /* copy PT_LOAD segments into the object's core */
    for (i = 0; i < object.ehdr->e_phnum; i++) {
	void *segstart;
	if (phdrs[i].p_type != PT_LOAD)
	    continue;

	segstart = loadaddr + phdrs[i].p_vaddr;

	if (phdrs[i].p_offset + phdrs[i].p_filesz > len) {
	    fprintf(stderr, "module truncated\n");
	    free(core);
	    return -ENOEXEC;
	}
	assert(phdrs[i].p_vaddr + phdrs[i].p_memsz <= max_addr);

	printf("copying file (0x%08lx,0x%lx) -> core (0x%08lx,0x%lx)\n",
		phdrs[i].p_offset, phdrs[i].p_filesz,
		phdrs[i].p_vaddr, phdrs[i].p_memsz);

	memcpy(segstart, object.buf + phdrs[i].p_offset, phdrs[i].p_filesz);
    }

    rc = processDynamicSection(dyn);

    return rc;
}

int Loader::processDynamicSection(Elf_Dyn *dyn)
{
    Elf_Rela *rela = NULL;
    Elf_Rela *jmprel = NULL;
    unsigned int n_rela = 0, n_pltrel = 0, i;
    int err;

    /* Scan the dynamic segment for interesting data.  */
    for (i = 0; dyn[i].d_tag != DT_NULL; i++) {
	void *addr = loadaddr + dyn[i].d_un.d_ptr;

	switch (dyn[i].d_tag) {
	case DT_STRTAB:
	    strtab = (char *)addr;
	    break;
	case DT_SYMTAB:
	    symtab = (Elf_Sym *)addr;
	    break;
	case DT_HASH:
	    /* slightly cheeky - nchain holds the number of symbol table
	     * entries */
	    n_sym = *((unsigned int *)addr + 1);
	    break;
	case DT_SYMENT:
	    if (dyn[i].d_un.d_val != sizeof(Elf_Sym)) {
		fprintf(stderr, "DT_SYMENT size mismatch.\n");
		return -ENOEXEC;
	    }
	    break;

	case DT_RELA:
	    rela = (Elf_Rela *)addr;
	    break;
	case DT_RELASZ:
	    n_rela = dyn[i].d_un.d_val / sizeof(Elf_Rela);
	    break;
	case DT_RELAENT:
	    if (dyn[i].d_un.d_val != sizeof(Elf_Rela)) {
		fprintf(stderr, "DT_RELAENT size mismatch.\n");
		return -ENOEXEC;
	    }
	    break;

	case DT_REL:
	case DT_RELSZ:
	case DT_RELENT:
	    fprintf(stderr, "REL relocations not supported on ppc64\n");
	    return -ENOEXEC;
	    break;

	case DT_JMPREL:
	    jmprel = (Elf_Rela *)addr;
	    break;
	case DT_PLTREL:
	    if (dyn[i].d_un.d_val != DT_RELA) {
		fprintf(stderr, "PLT relocations are not RELA type");
		return -ENOEXEC;
	    }
	    break;
	case DT_PLTRELSZ:
	    n_pltrel = dyn[i].d_un.d_val / sizeof(Elf_Rela);
	    break;

	case DT_INIT:
	    init = (void (*)(void))((char *)addr - loadaddr + kaddr) ;
	    printf("INIT at %lx\n", dyn[i].d_un.d_ptr);
	    break;
	}
    }

    /* check for required info */
    if (!symtab || !strtab || !n_sym || (n_rela && !rela)
	    || (n_pltrel && !jmprel)) {
	fprintf(stderr, "Object is missing required sections\n");
	return -ENOEXEC;
    }

    if (!init) {
	fprintf(stderr, "Object has no init function");
	return -ENOEXEC;
    }
    
    /* process relocations */
    for (i = 0; i < n_rela; i++) {
	if ((err = applyRelocation(rela + i)))
	    return err;
    }

    for (i = 0; i < n_pltrel; i++) {
	if ((err = applyRelocation(jmprel + i)))
	    return err;
    }

    return 0;
    
}

int Loader::applyRelocation(Elf_Rela *rel)
{
    Elf_Sym *sym = NULL;
    unsigned char type;
    unsigned int symnum;
    uval value = 0;
    uint64_t *location;
    struct SymbolResolver::syment *syment = NULL;
    
    location = (uint64_t *)(loadaddr + rel->r_offset);
        
    printf("relocation at %p: %lx %lx %lx %p\n", rel,
	    rel->r_offset, rel->r_info, rel->r_addend, location);

    type = ELF_R_TYPE(rel->r_info);
    symnum = ELF_R_SYM(rel->r_info);
    if (symnum) {
	sym = symtab + symnum;
	syment = resolve(strtab + sym->st_name);
	if (!syment) {
	    fprintf(stderr, "Unknown symbol %s\n", strtab + sym->st_name);
	    return -ENOENT;
	}
	value = syment->value;
    }

    value += rel->r_addend;

    switch (type) {
	case R_PPC64_RELATIVE:
	    *location = (uint64_t)(kaddr + value);
	    printf("\tRELATIVE: 0x%016lx -> 0x%016lx\n",
		    rel->r_addend, *location);
	    break;
	case R_PPC64_ADDR64:
	    printf("\tADDR64:   %s -> 0x%016lx + 0x%016lx\n",
		    strtab + sym->st_name,
		    rel->r_addend,
		    value);
	    *location = value;
	    break;
	case R_PPC64_JMP_SLOT:
	    printf("\tJMP_SLOT: %s -> 0x%016lx + 0x%016lx\n",
		    strtab + sym->st_name,
		    rel->r_addend,
		    value);
	    if (!syment->funcaddr) {
		fprintf(stderr, "symbol %s isn't an opd\n",
				strtab + sym->st_name);
		return -ENOENT;
	    }
	    location[0] = syment->funcaddr;
	    location[1] = syment->r2value;
	    /*
	    memcpy(location, (void *)value,
			       sizeof(struct ppc64_plt_entry));
	    */
	    {
	    struct ppc64_plt_entry *e = (struct ppc64_plt_entry *)location;
	    printf("\t%p {0x%016lx, 0x%016lx}\n",
		    e, (uval)e->function, (uval)e->r2val);
	    }
		    
	    break;
	default:
	    printf("\tUnknown relocation type %d\n", type);
	    return -ENOENT;
	    break;

    }
    
    return 0;
}

struct SymbolResolver::syment *Loader::resolve(const char *symbol)
{
    unsigned int i;
    SymbolResolver::syment *syment;

    syment = resolver->resolve(symbol);
    if (syment)
	return syment;

    /* look it up in the object's own symbol table */
    for (i = 0; i < n_sym; i++) {
	char *name = strtab + symtab[i].st_name;
	if (name && !strcmp(symbol, name)) {
	    /* @bug leak here! */
	    printf("found %s in local symtab\n", name);
	    syment = (SymbolResolver::syment *)
		malloc(sizeof(SymbolResolver::syment));
	    syment->name = strdup(symbol);
	    syment->value = symtab[i].st_value + (uval)loadaddr;
	    if (ELF_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
		syment->funcaddr = ((uval *)syment->value)[0];
		syment->r2value  = ((uval *)syment->value)[1];
	    }
	    return syment;
	}

    }
    return NULL;
}


int Loader::sanityCheck()
{
    if (memcmp(object.ehdr->e_ident, ELFMAG, SELFMAG))
	return 0;

    if (object.ehdr->e_ident[EI_CLASS] != RELOC_ELFCLASS)
	return 0;

    if (object.ehdr->e_ident[EI_DATA] != RELOC_ELFDATA)
	return 0;

    if (object.ehdr->e_ident[EI_VERSION] != EV_CURRENT)
	return 0;

    if (object.ehdr->e_type != ET_DYN) {
	fprintf(stderr, "Object file is not a dynamic object\n");
	return 0;
    }

    if (object.ehdr->e_machine != RELOC_EM) {
	fprintf(stderr, "Object file is for incorrect machine\n");
	return 0;
    }

    return -1;

}

void (*Loader::getInitFunction())(void)
{
    return init;
}
