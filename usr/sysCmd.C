 /******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysCmd.C,v 1.9 2005/08/22 17:15:10 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MACHINES 8

int verbose;
int thinOff;
int thinOn;
int traceOn;
int cmdOn;
int rebootOn;
int numbMachines;
char command[256];
long traceMask;

typedef struct {
    char name[64];
    char altName[64];
    char ip[64];
    int enabled;
} machine;

machine machines[MAX_MACHINES];

static void
print_usage()
{
    printf("sysCmd [--cmd \"command\"] [--help] [--m machine name|number|all] \n");
    printf("       [--reboot] [--t0] [--t1] [--trace mask] [--v]\n");
    printf("  sysCmd ssh commands to multiple nodes in a K42 cluster\n");
    printf("  --help prints this messages\n");
    printf("  --cmd executes command on each mahcine\n");
    printf("  --m machine name or number or all\n");
    printf("  --reboot shortcut to reboot machines, backgrounds ssh requests\n");
    printf("           so all sshs can occur without waiting for termination\n");
    printf("  --t0 shortcut to turn off thinwire on machines\n");
    printf("  --t1 shortcut to turn on thinwire on machines\n");
    printf("  --trace sets trace mask on each node, mask must be hex\n");
    printf("  --v causes sysCmd to print the actions it is taking\n");
    printf("  examples:\n");
    printf("    turn off thinwire on kxs1 and kxs4\n");
    printf("      sysCmd --m kxs1 --m kxs4 --t0\n");
    printf("    turn off thinwire on kxs1 and kxs4\n");
    printf("      sysCmd --m 1 --m 4 --t0\n");
    printf("    turn on thinwire on kxs1-kxs6 and kpem\n");
    printf("      sysCmd --m all --t1\n");
    printf("    set trace mask 0x4 on all machines\n");
    printf("      sysCmd --m all --cmd \"echo echo 0x4 > /ksys/traceMask\"\n");
}

void
init()
{
    verbose = 1;
    thinOff = 0;
    thinOn = 0;
    cmdOn = 0;
    rebootOn = 0;
    traceOn = 0;

    traceMask = 0;
    strcpy (command, "");

    strcpy (machines[0].name, "kpem");
    strcpy (machines[0].altName, "0");
    strcpy (machines[0].ip, "9.2.222.74");
    machines[0].enabled = 0;

    strcpy (machines[1].name, "kxs1");
    strcpy (machines[1].altName, "1");
    strcpy (machines[1].ip, "9.2.222.165");
    machines[1].enabled = 0;

    strcpy (machines[2].name, "kxs2");
    strcpy (machines[2].altName, "2");
    strcpy (machines[2].ip, "9.2.222.166");
    machines[2].enabled = 0;

    strcpy (machines[3].name, "kxs3");
    strcpy (machines[3].altName, "3");
    strcpy (machines[3].ip, "9.2.222.72");
    machines[3].enabled = 0;

    strcpy (machines[4].name, "kxs4");
    strcpy (machines[4].altName, "4");
    strcpy (machines[4].ip, "9.2.222.73");
    machines[4].enabled = 0;

    strcpy (machines[5].name, "kxs5");
    strcpy (machines[5].altName, "5");
    strcpy (machines[5].ip, "9.2.209.51");
    machines[5].enabled = 0;

    strcpy (machines[6].name, "kxs6");
    strcpy (machines[6].altName, "6");
    strcpy (machines[6].ip, "9.2.209.52");
    machines[6].enabled = 0;

    strcpy (machines[7].name, "kxs7");
    strcpy (machines[7].altName, "7");
    strcpy (machines[7].ip, "9.2.209.53");
    machines[7].enabled = 0;

    numbMachines = 8;
    if (numbMachines > MAX_MACHINES) {
	printf("error increase MAX_MACHINES\n");
	exit(1);
    }

}

int
main(int argc, char **argv)
{
    int index, i;
    char sysStr[256];
    char machName[64];

    init();

    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], "--help") == 0) {
	    print_usage();
	    return 0;
	} else if (strcmp(argv[index], "--cmd") == 0) {
	    strcpy(command, argv[++index]);
	    //sscanf(argv[++index], "%s", command);
	    cmdOn = 1;
	} else if (strcmp(argv[index], "--m") == 0) {
	    sscanf(argv[++index], "%s", machName);

	    if (strcmp(machName, "all") == 0) {
		for (i=0; i<numbMachines; i++) {
		    machines[i].enabled = 1;
		}
	    } else {
		for (i=0; i<numbMachines; i++) {
		    if ((strcmp(machines[i].name, machName) == 0) ||
			(strcmp(machines[i].altName, machName) == 0) ||
			(strcmp(machines[i].ip, machName) == 0)) {
			machines[i].enabled = 1;
		    }
		}
	    }
	} else if (strcmp(argv[index], "--reboot") == 0) {
	    rebootOn = 1;
	} else if (strcmp(argv[index], "--t0") == 0) {
	    if (thinOn == 1) {
		printf("error: can not turn thinwire both on and off\n");
		return -1;
	    }
	    thinOff = 1;
	} else if (strcmp(argv[index], "--t1") == 0) {
	    if (thinOff == 1) {
		printf("error: can not turn thinwire both on and off\n");
		return -1;
	    }
	    thinOn = 1;
	} else if (strcmp(argv[index], "--trace") == 0) {
	    traceMask = strtol(argv[++index], NULL, 16);
	    traceOn = 1;
	} else if (strcmp(argv[index], "--v") == 0) {
	    verbose = 1;
	    printf("verbose on\n");
	} else {
	    printf("Error: unknown option %s\n", argv[index]);
	    print_usage();
	    return -1;
	}
    }

    for (i=0; i<numbMachines; i++) {
	if (machines[i].enabled == 1) {
	    if (cmdOn) {
		sprintf(sysStr, "ssh %s \"%s\"",
			machines[i].ip, command);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
	    }
	    if (rebootOn) {
		sprintf(sysStr, "ssh %s \"echo 0 > /ksys/console\"",
			machines[i].ip);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
		sprintf(sysStr, "ssh %s \"echo X 1 > /ksys/console\"&",
			machines[i].ip);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
	    }
	    if (thinOff) {
		sprintf(sysStr, "ssh %s \"echo 0x0 > /ksys/wireDaemon\"",
			machines[i].ip);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
	    }
	    if (thinOn) {
		sprintf(sysStr, "ssh %s \"echo 0x1 > /ksys/wireDaemon\"",
			machines[i].ip);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
	    }
	    if (traceOn) {
		sprintf(sysStr, "ssh %s \"echo 0x%lx > /ksys/traceMask\"",
			machines[i].ip, traceMask);
		if (verbose == 1) {
		    printf("executing following %s:\n", machines[i].name);
		    printf("  %s\n", sysStr);
		}
		system(sysStr);
	    }
	}
    }

    return 0;
}
