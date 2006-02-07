/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinenv.C,v 1.6 2005/07/14 14:41:58 apw Exp $
 *****************************************************************************/

extern char *strncpy(char *, const char *, int);

#include <sys/sysIncs.H>
#include <stub/StubKBootParms.H>
#include <sys/systemAccess.H>

#include <stdio.h>
#include <unistd.h>

static int usage()
{
    printf("`thinenv' fetches K42 environment variables\n\n"
	   "Usage: thinenv LVALUE1 LVALUE2 ...\n"
           " -?  Show this help statement\n"
           " -l  Emit output in lvalue=rvalue form\n\n"
	   "Example: thinenv -l K42_PKGROOT\n\n");

    return 0;
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc = 0;
    char buf[1024] = {0,};
    int c, do_lvalue = 0;

    static char optstr[] = "hl";
    opterr = 1;

    if (argc < 2) {
        usage();
	return 1;
    }

    while ((c = getopt(argc, argv, optstr)) != EOF) {
        switch (c) {
        case 'h':
  	    usage();
            return 1;
        case 'l':
  	    do_lvalue = 1;
	    break;
        case '?':
  	    return 2;
        }
    }

    for (int i = optind; i < argc; i++) {
        rc = StubKBootParms::_GetParameterValue(argv[i], buf, sizeof(buf));

	if (!_SUCCESS(rc)) {
 	    continue;
	}

        if (do_lvalue) {
   	    printf("%s=%s\n", argv[i], buf);
        } else {
	    printf("%s\n", buf);
	}
    }

    return rc;
}
