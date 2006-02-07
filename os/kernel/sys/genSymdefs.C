/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genSymdefs.C,v 1.28 2004/03/14 14:50:11 mostrows Exp $
 * ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* We need __MINC in memoryMap.H, but don't want to bother with kinclude.H. */
#define _KS1(x) #x
#define _KS(x) _KS1(x)
/* used to include a machine specific file */
#define __MINC(fl)      _KS(arch/TARGET_MACHINE/fl)

#include "memoryMap.H"
#include "asmConstants.H"

#define MYCONCAT(a,b) a##b
#define ULL(a) MYCONCAT(a,ULL)

/* we need something representing an unsigned 64 bit, don't want to redefine
 * uval64 so we'll use a standard name
 */
typedef unsigned long long u_int64;

int main(int argc, char **argv)
{
    int user;
    u_int64 base;

#define ALLOC(var, align, size) \
    base = (base + (align - 1)) & ~(align - 1); \
    u_int64 var = base; \
    base += size

#define ALLOC_START(var, startAddr) \
    base = startAddr; \
    ALLOC(var, PG_SIZE, 0)

#define ALLOC_END(var) \
    ALLOC(var, PG_SIZE, 0); \
    base = 0

    if ((argc == 2) && (strcmp(argv[1], "-user") == 0)) {
	user = 1;
    } else if ((argc == 2) && (strcmp(argv[1], "-kernel") == 0)) {
	user = 0;
    } else {
	fprintf(stderr, "Usage: %s {-user|-kernel}\n");
	exit(-1);
    }

    /*
     * Processor-specific region for user address spaces.
     */
    ALLOC_START(userPSpecificSpace, USER_PSPECIFIC_BASE);

    ALLOC(USER_allocLocal,          8,       allocLocal_SIZE);
    ALLOC(USER_activeThrdCntLocal,  8,       activeThrdCntLocal_SIZE);
    ALLOC(USER_lTransTableLocal,    PG_SIZE, lTransTableLocal_SIZE);

    ALLOC_END(userPSpecificSpaceEnd);

    /*
     * Processor-specific region for the kernel address space.  On some
     * architectures (mips64, at least), exceptionLocal is assumed to
     * be first.
     */
    ALLOC_START(kernelPSpecificSpace, KERNEL_PSPECIFIC_BASE);

    ALLOC(KERNEL_exceptionLocal,     PG_SIZE, exceptionLocal_SIZE);
    ALLOC(KERNEL_allocLocal,         8,       allocLocal_SIZE);
    ALLOC(KERNEL_activeThrdCntLocal, 8,       activeThrdCntLocal_SIZE);
    ALLOC(KERNEL_lTransTableLocal,   PG_SIZE, lTransTableLocal_SIZE);
    ALLOC_END(kernelPSpecificSpaceEnd);


    /*
     * Processor-specific region common to user and kernel address spaces.
     */
    ALLOC_START(commonPSpecificSpace, COMMON_PSPECIFIC_BASE);

    ALLOC(COMMON_kernelInfoLocal,   PG_SIZE, kernelInfoLocal_SIZE);
    ALLOC(COMMON_extRegsLocal,      PG_SIZE, extRegsLocal_SIZE);

    ALLOC_END(commonPSpecificSpaceEnd);

#ifdef TLB_SET_SIZE
    /*
     * On some architectures it's important to avoid clustering well-known
     * and frequently-used addresses in overloaded TLB congruence classes.
     * If the architecture's memoryMap.H file defines a TLB_SET_SIZE, and if
     * the user, kernel, and common processor-specific regions will fit in
     * one set, we try to pack them toward the end of a TLB set.
     */
    u_int64 userSize, kernelSize, commonSize;
    u_int64 userOffset, kernelOffset, commonOffset;

    userSize = userPSpecificSpaceEnd - userPSpecificSpace;
    kernelSize = kernelPSpecificSpaceEnd - kernelPSpecificSpace;
    commonSize = commonPSpecificSpaceEnd - commonPSpecificSpace;

    if ((userSize + kernelSize + commonSize) < TLB_SET_SIZE) {
	commonOffset = TLB_SET_SIZE - commonSize;
	kernelOffset = commonOffset - kernelSize;
	userOffset = kernelOffset - userSize;

	commonOffset = (commonOffset-commonPSpecificSpace) & (TLB_SET_SIZE-1);
	kernelOffset = (kernelOffset-kernelPSpecificSpace) & (TLB_SET_SIZE-1);
	userOffset = (userOffset-userPSpecificSpace) & (TLB_SET_SIZE-1);

	userPSpecificSpace		+= userOffset;
	USER_allocLocal			+= userOffset;
	USER_activeThrdCntLocal		+= userOffset;
	USER_lTransTableLocal		+= userOffset;
	userPSpecificSpaceEnd		+= userOffset;

	kernelPSpecificSpace		+= kernelOffset;
	KERNEL_exceptionLocal		+= kernelOffset;
	KERNEL_allocLocal		+= kernelOffset;
	KERNEL_activeThrdCntLocal	+= kernelOffset;
	KERNEL_lTransTableLocal		+= kernelOffset;
	kernelPSpecificSpaceEnd		+= kernelOffset;

	commonPSpecificSpace		+= commonOffset;
	COMMON_kernelInfoLocal		+= commonOffset;
	COMMON_extRegsLocal		+= commonOffset;
	commonPSpecificSpaceEnd		+= commonOffset;
    }
#endif // TLB_SET_SIZE

    if (user) {
	printf("allocLocal 0x%llx\n",              USER_allocLocal);
	printf("activeThrdCntLocal 0x%llx\n",      USER_activeThrdCntLocal);
	printf("lTransTableLocal 0x%llx\n",        USER_lTransTableLocal);
    } else {
	printf("allocLocal 0x%llx\n",              KERNEL_allocLocal);
	printf("activeThrdCntLocal 0x%llx\n",      KERNEL_activeThrdCntLocal);
	printf("lTransTableLocal 0x%llx\n",        KERNEL_lTransTableLocal);
	printf("exceptionLocal 0x%llx\n",          KERNEL_exceptionLocal);

	/*
	 * Boundaries for the kernel processor-specific region are
	 * needed in the kernel address space for initialization.
	 */
	printf("kernelPSpecificSpace 0x%llx\n",    kernelPSpecificSpace);
	printf("kernelPSpecificSpaceEnd 0x%llx\n", kernelPSpecificSpaceEnd);
    }

    printf("kernelInfoLocal 0x%llx\n",         COMMON_kernelInfoLocal);
    // LinuxVPInfo is a subset of KernelInfo local used by
    // linux code, we must define a seperate symbol for it to ensure
    // that we can refer to it via a C-safe type.
    printf("linuxVPInfo 0x%llx\n",	       COMMON_kernelInfoLocal);
    printf("extRegsLocal 0x%llx\n",            COMMON_extRegsLocal);

    /*
     * Boundaries for both user and common processor-specific regions
     * are needed in both user and kernel address spaces for the sake
     * of creating new processes.
     */
    printf("userPSpecificSpace 0x%llx\n",      userPSpecificSpace);
    printf("userPSpecificSpaceEnd 0x%llx\n",   userPSpecificSpaceEnd);
    printf("commonPSpecificSpace 0x%llx\n",    commonPSpecificSpace);
    printf("commonPSpecificSpaceEnd 0x%llx\n", commonPSpecificSpaceEnd);

    return 0;
}
