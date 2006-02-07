/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ModuleLoader.C,v 1.2 2005/06/28 19:42:49 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/systemAccess.H>
#include <stub/StubFRKernelPinned.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

#include "Loader.H"
#include "SymbolResolver.H"

/* Slurp a file into memory. Pointer must be free()d */
char *load_file(const char *path, int *len)
{
    struct stat statbuf;
    int fd;
    char *buf;

    if (stat(path, &statbuf)) {
	perror("stat");
	return NULL;
    }

    if (!(fd = open(path, O_RDONLY))) {
	perror("open");
	return NULL;
    }

    if (!(buf = (char *)malloc(statbuf.st_size))) {
	perror("malloc");
	return NULL;
    }

    *len = statbuf.st_size;

    if (read(fd, buf, statbuf.st_size) != statbuf.st_size) {
	perror("read");
	free(buf);
	return NULL;
    }

    if (close(fd))
	perror("close");

    return buf;
}

/**
 * Callback to alloc memory for the loaded module
 */
void *moduleAlloc(uval size, uval &kaddr)
{
    SysStatus rc;
    ObjectHandle frOH;
    uval uaddr;

    rc = StubFRKernelPinned::_Create(frOH, kaddr, size);
    if (_FAILURE(rc)) {
	printf("FRKernelPinned::_Create() failed\n");
	return NULL;
    }

    /* Create a Region to allow the process to reference the memory */
    rc = StubRegionDefault::_CreateFixedLenExt(uaddr, size, 0, frOH, 0,
	    AccessMode::writeUserWriteSup, 0, RegionType::UseSame);

    if (_FAILURE(rc)) {
	printf("_CreateFixedLen() failed: 0x%016lx", rc);
	return NULL;
    }
    
    printf("moduleAlloc: 0x%016lx -> 0x%016lx\n", uaddr, kaddr);

    return (void *)uaddr;


}

int main(int argc, char **argv)
{
    NativeProcess();

    Loader *l;
    SymbolResolver *r;
    int len;
    char *buf;
    uval module_init;
    SysStatus rc;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s <object-file>\n", argv[0]);
	return EXIT_FAILURE;
    }

    if (!(buf = load_file(argv[1], &len))) {
	return EXIT_FAILURE;
    }

    r = new SymbolResolver();

    if (r->init()) {
	fprintf(stderr, "Couldn't init resolver\n");
	return EXIT_FAILURE;
    }


    l = new Loader(r, moduleAlloc);

    if (l->loadModule(buf, len)) {
	fprintf(stderr, "relocation failed\n");
	free(buf);
	return EXIT_FAILURE;
    }

    module_init = (uval)(l->getInitFunction());

    printf("module entry point: 0x%016lx\n", module_init);

    rc = StubFRKernelPinned::_InitModule(module_init);

    if (_FAILURE(rc)) {
	printf("FRKernelPinned::_InitModule() failed\n");
	return EXIT_FAILURE;
    }

    delete(l);
    delete(r);

    free(buf);

    return 0;
}
