/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Loader.H,v 1.1 2005/06/07 03:46:39 jk Exp $
 *****************************************************************************/

#ifndef __LOADER_H_
#define __LOADER_H_

#include "SymbolResolver.H"
#include <elf.h>

#define ARCH_ppc64 1

#if defined(ARCH_ppc64)
# define RELOC_ELFCLASS ELFCLASS64
# define RELOC_ELFDATA  ELFDATA2MSB
# define RELOC_EM	EM_PPC64

#elif defined(ARCH_x86)
# define RELOC_ELFCLASS	ELFCLASS32
# define RELOC_ELFDATA	ELFDATA2LSB
# define RELOC_EM	EM_386

#else
# error no arch defined!
#endif

#if RELOC_ELFCLASS == ELFCLASS64
#define Elf_Ehdr	Elf64_Ehdr
#define Elf_Shdr	Elf64_Shdr
#define Elf_Rela	Elf64_Rela
#define Elf_Sym		Elf64_Sym
#define Elf_Phdr	Elf64_Phdr
#define Elf_Dyn		Elf64_Dyn
#define ELF_R_SYM	ELF64_R_SYM
#define ELF_R_TYPE	ELF64_R_TYPE
#define ELF_R_INFO	ELF64_R_INFO
#define ELF_ST_TYPE	ELF64_ST_TYPE
#define Elf_PTYPE	uint64_t

#else
#define Elf_Ehdr	Elf32_Ehdr
#define Elf_Shdr	Elf32_Shdr
#define Elf_Rela	Elf32_Rela
#define Elf_Sym 	Elf32_Sym
#define ELF_R_SYM	ELF32_R_SYM
#define ELF_R_TYPE	ELF32_R_TYPE
#define ELF_R_INFO	ELF32_R_INFO
#define Elf_PTYPE	uint32_t

#endif

/**
 * A class to load and relocate an ELF object.
 * @todo handle RELOC_REL relocations (not required for ppc64)
 */
class Loader {
public:

    DEFINE_GLOBAL_NEW(Loader);

    /**
     * Construct a Loader. init() must be called before any other
     * functions.
     * @param r A SymbolResolver to look up unresolved strings in the module
     * @param alloc A function to allocate module memory
     */
    Loader(SymbolResolver *r, void *(*alloc)(uval, uval&));

    /**
     * Initialise the Relocator with an ELF object.
     * @param buf A pointer to a buffer containing an ELF object
     * @param len The length of buf
     * @return zero on success, non-zero on failure
     */
    int loadModule(char *buf, unsigned int len);

    /**
     * Get the entry point for the module
     * @return the entry point of the loaded module
     */
    void (*getInitFunction())(void);

private:

    /**
     * Reference to the object file in memory, before being processed by the
     * loader
     */
    union {
	Elf_Ehdr	*ehdr;
	char		*buf;
    } object;

    /** size of the object's file */
    unsigned int	objsize;
    
    /** pointer to module loaded in memory */
    char		*core;

    /** the core's virtual base address */
    char		*loadaddr;

    /** the object's address in the kernel */
    uval		kaddr;

    /** the object's symbol table */
    Elf_Sym		*symtab;

    /** number of symbols in the hash table */
    unsigned int	n_sym;

    /** the object's string table */
    char 		*strtab;

    /** the module's entry point */
    void 		(*init)(void);

    /** the resolver to look up function names */
    SymbolResolver	*resolver;

    /** a function to allocate kernel memory for the module */
    void *(*alloc)(uval size, uval &kaddr);

    /**
     * Perform sanity checks on an ELF object
     * @return zero on success, non-zero on failure
     */
    int sanityCheck();

    /**
     * Process a module's dynamic section
     */
    int processDynamicSection(Elf_Dyn *dyn);

    /**
     * Process a single relocation
     */
    int applyRelocation(Elf_Rela *rel);

    struct SymbolResolver::syment *resolve(const char *symbol);
};

#endif /* __LOADER_H_ */
