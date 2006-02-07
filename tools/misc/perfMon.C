/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: perfMon.C,v 1.6 2002/11/25 21:51:40 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USAGE "usage:\n perfMon [filename]\n filename defaults to cpu.log\n"

#define MAX_MONITOR_SYMS 32
#define INFINITY 999999999
#define MAX_REENTRY 8

typedef struct {
    int total, max, min, count, numbReentries;
    int startTime[MAX_REENTRY], startSP[MAX_REENTRY];
    char sym[16], functionName[64], startSym[16], endSym[16];
} monintorSym;

typedef struct {
    monintorSym sym[MAX_MONITOR_SYMS];
    int numbSyms;
} perfMon;

perfMon PerfMon;

int
main(int argc, char **argv)
{
    char filename[32], garb[32], sym[16];
    char next_line[512];
    FILE *fp;
    int i, j, val, diff, sp;

    if (argc > 2) {
	printf("%s",USAGE);
	return(-1);
    }

    if (argc == 1) {
	strcpy(filename, "cpu.log");
    } else {
	strcpy(filename, argv[1]);
    }

    if ((fp = fopen(filename, "r")) == NULL) {
	printf("error: couldn't open %s for reading\n", filename);
	return(-1);
    }

    // first phase get all the we are going to monitor
    PerfMon.numbSyms = 0;
    while (fgets(next_line, 512, fp) != NULL) {
	if (strstr(next_line, "kitchCode") != NULL) {
	    sscanf(next_line, "%s %s %s", garb,
		   PerfMon.sym[PerfMon.numbSyms].sym,
		   PerfMon.sym[PerfMon.numbSyms].functionName);
	    strcpy(PerfMon.sym[PerfMon.numbSyms].startSym,
		   PerfMon.sym[PerfMon.numbSyms].sym);
	    strcat(PerfMon.sym[PerfMon.numbSyms].startSym, "s");
	    strcpy(PerfMon.sym[PerfMon.numbSyms].endSym,
		   PerfMon.sym[PerfMon.numbSyms].sym);
	    strcat(PerfMon.sym[PerfMon.numbSyms].endSym, "e");
	    PerfMon.sym[PerfMon.numbSyms].total = 0;
	    PerfMon.sym[PerfMon.numbSyms].max = 0;
	    PerfMon.sym[PerfMon.numbSyms].min = INFINITY;
	    PerfMon.sym[PerfMon.numbSyms].count = 0;
	    PerfMon.sym[PerfMon.numbSyms].numbReentries = 0;
	    for (i=0;i<MAX_REENTRY;i++) {
		PerfMon.sym[PerfMon.numbSyms].startTime[i] = 0;
		PerfMon.sym[PerfMon.numbSyms].startSP[i] = 0;
	    }
	    PerfMon.numbSyms = PerfMon.numbSyms + 1;
	    printf("tracing %s\n", PerfMon.sym[PerfMon.numbSyms].sym);
	}
	if (strstr(next_line, "kitchStart") != NULL) {
	    break;
	}
    }

    // next phase figure out when they were each called
    while (fgets(next_line, 512, fp) != NULL) {
	if (strstr(next_line, "kitchMonitor") != NULL) {
	    sscanf(next_line, "%s %s %d %x", garb, sym, &val, &sp);

	    i=0;
	    while(i<PerfMon.numbSyms) {
		if (strcmp(PerfMon.sym[i].startSym, sym) == 0) {
		    j=0;
		    while ((j<MAX_REENTRY) &&
			   (PerfMon.sym[i].startSP[j] != 0)) {
			if (PerfMon.sym[i].startSP[j] == sp) {
			    printf("error: don't know how to deal with re-entries");
			    printf("occurred for %s\n",PerfMon.sym[i].functionName);
			    break;
			    //exit(-1);
			}
			j++;
		    }
		    PerfMon.sym[i].startTime[j] = val;
		    PerfMon.sym[i].startSP[j] = sp;
		    PerfMon.sym[i].numbReentries += 1;
		    break;
		}
		if (strcmp(PerfMon.sym[i].endSym, sym) == 0) {
		    j=0;
		    while ((j<MAX_REENTRY) &&
			   (PerfMon.sym[i].startSP[j] != 0)) {
			if (PerfMon.sym[i].startSP[j] == sp) {
			    diff = val - PerfMon.sym[i].startTime[j];
			    PerfMon.sym[i].total += diff;

			    if (diff > 100000) {
				printf("diff greater\n");
			    }
			    if (diff < PerfMon.sym[i].min) {
				PerfMon.sym[i].min = diff;
			    }
			    if (diff > PerfMon.sym[i].max) {
				PerfMon.sym[i].max = diff;
			    }
			    PerfMon.sym[i].count += 1;

			    PerfMon.sym[i].numbReentries -= 1;

			    // update list don't forget to bring start time
			    PerfMon.sym[i].startSP[j] =
				PerfMon.sym[i].startSP[
				    PerfMon.sym[i].numbReentries];

			    PerfMon.sym[i].startTime[j] =
				PerfMon.sym[i].startTime[
				    PerfMon.sym[i].numbReentries];

			    PerfMon.sym[i].startSP[
				PerfMon.sym[i].numbReentries] = 0;

			    break;
			}
			j++;
		    }
		    if (j==MAX_REENTRY) {
			printf("error: exit seen without entry for %s\n",
			       PerfMon.sym[i].functionName);
			printf("       %s", next_line);
		    }
		    break; // break from while(i<PerfMon.numbSyms)
		}
		i++;
	    }
	    if (i>PerfMon.numbSyms) {
		printf("error: unknown symbol %s\n", sym);
		exit(-1);
	    }
	}
    }

    fclose(fp);
    // last phase print out results

    for (i=0; i<PerfMon.numbSyms; i++) {
	printf("results for function %s:\n",
	       PerfMon.sym[i].functionName);
	printf(" average instruction count: %d\n",
	       PerfMon.sym[i].total/PerfMon.sym[i].count);
	printf(" number of occurrences: %d\n",PerfMon.sym[i].count);
	printf(" max: %d\n",PerfMon.sym[i].max);
	printf(" min: %d\n",PerfMon.sym[i].min);
    }

    return 0;
}
