/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mkboot.c,v 1.7 2000/05/11 11:48:49 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: mkboot program for aix - takes kernel and user bits
 *                     and stuffs them together.  Currently (11-09-97) it puts
 *                     the bits at the end of the text segment
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mkboot.h"

char *bits_file;
char *kern_file;
char *boot_file;

struct stat bits_info;
struct stat kern_info;

int text_file_size;
int text_file_offset;
int data_file_offset;
int old_data_file_offset;
int amount_push_back;

/* FIXME - these need to obtained from one consistent source
 *         this constant is in two or three different places
 */
#define TEXT_VADDR 0xC0110000
#define DATA_VADDR 0xC0150000
#define BITS_VADDR 0xC0140000 /* this needs to be passed in from kernel.script */

// FIXME these need to obtained from standard locations
#define PAGE_SIZE 4096
#define PAGE_MASK 0xfffff000

int debug_level = 1;

dump_elfhdr(Elf32_Ehdr *eh32)
{
  printf("ident %x type %x machine %x version %x\n entry %x phoff %x shoff %x flags %x\n ehsize %x phentsize %x phnum %x shentsize %x\n  shnum %x shstrndx %x\n",
	 eh32->e_ident[EI_NIDENT], eh32->e_type, eh32->e_machine,
	 eh32->e_version, eh32->e_entry, eh32->e_phoff, eh32->e_shoff,
	 eh32->e_flags, eh32->e_ehsize, eh32->e_phentsize, eh32->e_phnum,
	 eh32->e_shentsize, eh32->e_shnum, eh32->e_shstrndx);

}

dump_phdr(Elf32_Phdr *phdr)
{
  printf("type %x offset %x vaddr %x paddr %x\n filesz %x memsz %x flags %x align %x\n",
	 phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr,
	 phdr->p_filesz, phdr->p_memsz, phdr->p_flags, phdr->p_align);
}

dump_shdr(Elf32_Shdr *shdr)
{
  printf("name %x type %x flags %x addr %x offset %x\n size %x link %x info %x addalign %x entsize %x\n",
	 shdr->sh_name, shdr->sh_type, shdr->sh_flags, shdr->sh_addr,
	 shdr->sh_offset, shdr->sh_size, shdr->sh_link, shdr->sh_info,
	 shdr->sh_addralign,shdr->sh_entsize);
}

int
check_elf_hdr_validity(Elf32_Ehdr *eh32)
{
    int valid;				/* flag indicating validity of file */

    if (debug_level == 2) {
        printf("\nDumping shdr information\n");
        dump_elfhdr(eh32);
	printf("\n\n");
    }
    /* consistency checks */
    valid = 1;
    valid = valid & eh32->e_ident[EI_MAG0]==0x7f;
    valid = valid & eh32->e_ident[EI_MAG1]=='E';
    valid = valid & eh32->e_ident[EI_MAG2]=='L';
    valid = valid & eh32->e_ident[EI_MAG3]=='F';

    if (!valid) {
	printf("loader: not an ELF format file.\n");
	return -1;
    }

    valid = valid & eh32->e_ident[EI_DATA]==ELFDATA2MSB;
    if (!valid) {
	printf("loader: only support big-endian executables.\n");
	return -1;
    }

    valid = valid & eh32->e_ident[EI_VERSION]==EV_CURRENT;
    if (!valid) {
	printf("loader: only support current version ELF files.\n");
	return -1;
    }

    if (eh32->e_phoff==0) {
	printf("loader: null program header -> program not loadable.\n");
	return -1;
    }
    return 0;
}

/* fix the program header that represetns the text segment.
 * make it large enough to contain the extra bits we are going
 * to put in - fix both the memory and file size
 */
int
adj_phdr(Elf32_Ehdr *eh32)
{
    Elf32_Phdr *phdr;
    int phdr_num;
    int i;
    int new_text_size;
    int text_mem_size;

    phdr = (Elf32_Phdr *)(kern_file+eh32->e_phoff);
    phdr_num = eh32->e_phnum;

    if (debug_level == 2) {
        printf("Dumping phdr information\n");
    }
    /* look for text segment and data segment info */
    for (i=0; i<phdr_num; i++,phdr++) {
        if (debug_level == 2) {
	    dump_phdr(phdr);
	}
        if (phdr->p_vaddr == TEXT_VADDR) {
	  text_file_size = phdr->p_filesz;
	  text_mem_size = phdr->p_memsz;
	  text_file_offset = phdr->p_offset;
	}
        if (phdr->p_vaddr == DATA_VADDR) {
	  old_data_file_offset = phdr->p_offset;
	}
    }

    if (old_data_file_offset < text_file_offset) {
      fprintf(stderr, "error data segment before text segment\n");
      exit(-1);
    }

    /* now go through and adjust text sizes and data offset */
    phdr = (Elf32_Phdr *)(kern_file+eh32->e_phoff);
    for (i=0; i<phdr_num; i++,phdr++) {
        if (phdr->p_vaddr == TEXT_VADDR) {
	    new_text_size = PAGE_ROUND_UP((BITS_VADDR - TEXT_VADDR) + bits_info.st_size);
	    phdr->p_filesz = new_text_size;
	    phdr->p_memsz = new_text_size;
	}
        if (phdr->p_vaddr == DATA_VADDR) {
	  data_file_offset = text_file_offset + new_text_size;
	  amount_push_back = data_file_offset - old_data_file_offset;
	  phdr->p_offset = data_file_offset;
	}
    }

    if (debug_level == 2) {
        printf("\n\n");
    }
    return 0;
}

/* we have put in extra space in the last section in the text segment
 * we not need to go through the rest of the section headers and indicate
 * this by pushing back where they start
 * we also need to increase the size of the last section in the text segment
 */
adj_shdr(Elf32_Ehdr *eh32)
{
    Elf32_Shdr *shdr;
    Elf32_Shdr *last_shdr_before_data;
    int i;

    shdr = (Elf32_Shdr *)((eh32->e_shoff+kern_file));
    last_shdr_before_data = NULL;

    if (debug_level == 2) {
        printf("Dumping shdr information\n");
    }
    for (i=0; i<eh32->e_shnum; i++,shdr++) {
        if (debug_level == 2) {
	    dump_shdr(shdr);
	}
        if (shdr->sh_offset >= old_data_file_offset) {
	    shdr->sh_offset += amount_push_back;
	}
	else if (shdr->sh_offset > text_file_offset) {
	    if (last_shdr_before_data == NULL) {
	        last_shdr_before_data = shdr;
	    }
	    else if (shdr->sh_addr > last_shdr_before_data->sh_addr) {
	        last_shdr_before_data = shdr;
	    }
	}
    }
    if (debug_level == 2) {
        printf("\n\n");
    }
    last_shdr_before_data->sh_size = data_file_offset - last_shdr_before_data->sh_offset;

}



main(int argc, char **argv)
{
    FILE *fp;
    int fd, i;
    Elf32_Ehdr *eh32;
    int check_size;

    char bits_filename[128];
    char kern_filename[128];
    char out_filename[128];

    char err_str[128];

    if ((argc < 4) || (argc > 5)) {
      fprintf(stderr, "usage mkboot kernel_file user_servers_file [debug_level]\n");
      exit(-1);
    }
    strcpy(kern_filename, argv[1]);
    strcpy(bits_filename, argv[2]);
    strcpy(out_filename, argv[3]);

    debug_level = 0;
    if (argc == 5)
      debug_level = atoi(argv[4]);

    stat(bits_filename, &bits_info);
    stat(kern_filename, &kern_info);

    if (debug_level)
      printf("bits size %d kern size %d\n", bits_info.st_size, kern_info.st_size);

    kern_file = (char *)malloc(kern_info.st_size);
    bits_file = (char *)malloc(bits_info.st_size);

    fd = open(bits_filename,  O_RDONLY);
    if (fd == -1) {
      sprintf(err_str, "error opening %s", bits_filename);
      perror(err_str);
      exit(-1);
    }

    check_size = read (fd, bits_file, bits_info.st_size);
    if (debug_level)
      printf("bits_size %d\n", check_size);
    close(fd);

    fd = open(kern_filename,  O_RDONLY);
    if (fd == -1) {
      sprintf(err_str, "error opening %s", kern_filename);
      perror(err_str);
      exit(-1);
    }

    check_size = read (fd, kern_file, kern_info.st_size);
    if (debug_level)
      printf("read in size %d\n", check_size);
    close(fd);

    eh32 = (Elf32_Ehdr *)kern_file;
    if (check_elf_hdr_validity(eh32) == -1) {
      printf("error in elf file\n");
      exit(-1);
    }

    adj_phdr(eh32);
    adj_shdr(eh32);

    fp = fopen(out_filename, "w");
    fclose(fp);

    if (debug_level)
      printf("amount push back %d\n", amount_push_back);
    eh32->e_shoff += amount_push_back;

    /* FIXME we need a real calculation here */
    if (debug_level)
      printf("boot file size %d\n", (kern_info.st_size + amount_push_back));
    boot_file = (char *)malloc(kern_info.st_size + amount_push_back);
    bzero(boot_file, kern_info.st_size + amount_push_back);

    /* write in up to the kernel text segment */
    bcopy(kern_file, boot_file, old_data_file_offset);

    /* now put our new bits in */
    bcopy(bits_file,
	  boot_file+(text_file_offset + (BITS_VADDR - TEXT_VADDR)),
	  bits_info.st_size);

    /* now put in the kernel data offset by the new amount */
    bcopy(kern_file+old_data_file_offset,
	  boot_file+data_file_offset,
	  kern_info.st_size-old_data_file_offset);


    fd = open(out_filename,  O_RDWR | O_CREAT);
    check_size = write (fd, boot_file, kern_info.st_size+amount_push_back);
    if (debug_level)
      printf("write in size %d\n", check_size);

    close(fd);
}
