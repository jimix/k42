/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dqmon.C,v 1.4 2005/06/28 19:42:45 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:  program to display DispatchQueue statistics
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <sys/systemAccess.H>
#include <sys/ResMgrWrapper.H>
#include <sys/KernelScheduler.H>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(char *prog)
{
    printf("Usage: %s [<msec>]\n"
	       "\tDisplay DispatchQueue statistics.\n",
	    prog);
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc;
    int msec;
    uval statsRegionAddr, statsRegionSize, statsSize;
    VPNum numProcessors, pp, mypp;
    KernelScheduler::Stats *stats;
    uval load, interval;

    if (argc > 2) {
	usage(argv[0]);
	return -1;
    }

    msec = -1;
    if (argc > 1) {
	msec = atoi(argv[1]);
	if (msec <= 0) {
	    usage(argv[0]);
	    return -1;
	}
    }

    rc = DREFGOBJ(TheResourceManagerRef)->
	    mapKernelSchedulerStats(statsRegionAddr,
				    statsRegionSize,
				    statsSize);
    passertMsg(_SUCCESS(rc), "mapKernelSchedulerStats() failed.\n");
    numProcessors = _SGETUVAL(DREFGOBJ(TheProcessRef)->ppCount());
    mypp = kernelInfoLocal.physProc;

    stats = (KernelScheduler::Stats *) (statsRegionAddr + (0*statsSize));
    interval = stats->smoothingInterval;
    printf("Smoothing interval %15ld.\n", interval);

    for (;;) {
	for (pp = 0; pp < numProcessors; pp++) {
	    stats = (KernelScheduler::Stats *)
			(statsRegionAddr + (pp*statsSize));
	    load = stats->runnableDispatcherCount;
	    load -= 2;		// don't count idle and dummy dispatchers
	    if (pp == mypp) {
		load -= 1;	// don't count our own dispatcher
	    }
	    printf("%3ld", load);
	}
	printf("     ");
	for (pp = 0; pp < numProcessors; pp++) {
	    stats = (KernelScheduler::Stats *)
			(statsRegionAddr + (pp*statsSize));
	    printf("%6ld", stats->smooth[4].runnableWeightAccum / interval);
	}

	printf("    ");
	for (pp = 0; pp < numProcessors; pp++) {
	    stats = (KernelScheduler::Stats *)
			(statsRegionAddr + (pp*statsSize));
	    printf("%4.4f  ", (double)stats->smooth[4].dispatchTimeAccum / 
		   (double)interval);
	}

	printf("\n");

	if (msec <= 0) break;

	usleep(1000*msec);
    }

    return 0;
}
