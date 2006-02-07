/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmattest.c,v 1.2 2004/12/20 20:23:00 marc Exp $
 *****************************************************************************/

/* Example of using hugepage in user application using Sys V shared memory
 * system calls.  In this example, app is requesting memory of size 256MB that
 * is backed by huge pages.  Application uses the flag SHM_HUGETLB in shmget
 * system call to informt the kernel that it is requesting hugepages.  For
 * IA-64 architecture, Linux kernel reserves Region number 4 for hugepages.
 * That means the addresses starting with 0x800000....will need to be
 * specified.
 */
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

extern int errno;
#define SHM_HUGETLB 04000
//#define LPAGE_SIZE      (256UL*1024UL*1024UL)
#define LPAGE_SIZE      (16UL*1024UL*1024UL)
#define         dprintf(x)  printf(x)
//#define ADDR (0x8000000000000000UL)
#define ADDR 0				// maa let shmat decide
int
main()
{
        int shmid;
        int     i;
        volatile        char    *shmaddr;

        if ((shmid =shmget(IPC_PRIVATE, LPAGE_SIZE,
			   SHM_HUGETLB|IPC_CREAT|SHM_R|SHM_W )) < 0) {
	    perror("Failure:");
	    exit(1);
        }
        printf("shmid: 0x%x\n", shmid);
        shmaddr = shmat(shmid, (void *)ADDR, SHM_RND) ;
        if (errno != 0) {
                perror("Shared Memory Attach Failure:");
                exit(2);
        }
        printf("shmaddr: %p\n", shmaddr);

        dprintf("Starting the writes:\n");
        for (i=0;i<LPAGE_SIZE;i++) {
                shmaddr[i] = (char) (i);
                if (!(i%(1024*1024))) dprintf(".");
        }
        dprintf("\n");
        dprintf("Starting the Check...");
        for (i=0; i<LPAGE_SIZE;i++)
                if (shmaddr[i] != (char)i)
                        printf("\nIndex %d mismatched.", i);
        dprintf("Done.\n");
        if (shmdt((const void *)shmaddr) != 0) {
                perror("Detached Failure:");
                exit (3);
        }
	return 0;
}
