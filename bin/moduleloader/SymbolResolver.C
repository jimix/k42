/******************************************************************************
* K42: (C) Copyright IBM Corp. 2005.
* All Rights Reserved
*
* This file is distributed under the GNU LGPL. You should have
* received a copy of the license along with K42; see the file LICENSE.html
* in the top-level directory for more details.
*
* $Id: SymbolResolver.C,v 1.1 2005/06/07 03:46:39 jk Exp $
*****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "SymbolResolver.H"

#ifndef MAP_FILE
#define MAP_FILE "boot_image.map"
#endif

#define SYM_LEN 256

SymbolResolver::SymbolResolver()
{
    symbols = NULL;
    n_symbols = 0;
    alloc_symbols = 0;
}

int SymbolResolver::init()
{
    FILE *fd;
    char name[SYM_LEN];
    struct syment syment;

    if (!(fd = fopen(MAP_FILE, "r"))) {
	perror("open");
	return errno;
    }

    syment.name = name;

    while (!feof(fd)) {
	int rc = fscanf(fd, "%s 0x%lx 0x%lx 0x%lx\n",
		syment.name, &syment.value, &syment.funcaddr, &syment.r2value);
	if (rc == 0)
	    break;

	if (rc != 4)
	    syment.funcaddr = syment.r2value = 0;

	addSymbol(&syment);
    }

    fclose(fd);

    printf("got %d symbols\n", n_symbols);

    return 0;
}

struct SymbolResolver::syment *SymbolResolver::resolve(const char *symbol)
{
    unsigned int i;
    for (i = 0; i < n_symbols; i++)
	if (!strcmp(symbols[i].name, symbol))
	    return symbols + i;
    return 0;
}

void SymbolResolver::dump()
{
    unsigned int i;
    for (i = 0; i < n_symbols; i++) {
	if (symbols[i].funcaddr)
	    printf("%s -> %lx\n", symbols[i].name, symbols[i].value);
	else
	    printf("%s -> %lx (%lx, %lx)\n", symbols[i].name,
		    symbols[i].value, symbols[i].funcaddr, symbols[i].r2value);
    }
}

#define max(a,b) ((a) > (b) ? (a) : (b))

int SymbolResolver::addSymbol(struct SymbolResolver::syment *syment)
{
    if (n_symbols == alloc_symbols) {
	alloc_symbols = max(alloc_symbols * 2, 256);

	symbols = (struct syment *)realloc(symbols,
		alloc_symbols * sizeof(struct syment));
	if (!symbols) {
	    perror("realloc");
	    return -1;
	}
    }

    symbols[n_symbols].name	= strdup(syment->name);
    symbols[n_symbols].value	= syment->value;
    symbols[n_symbols].funcaddr	= syment->funcaddr;
    symbols[n_symbols].r2value	= syment->r2value;
    n_symbols++;

    return 0;
}

