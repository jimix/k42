/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dumpTraceStrings.C,v 1.2 2004/04/12 13:09:18 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Used to produce a listing of the major and minor IDs,
 * and other tracing information - used in building the kmon gui tool.
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "sys/hostSysTypes.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"

int
main(void)
{
    FILE *fp;
    uval i,j;

    fp = fopen("traceStrings.java", "w");
    if (fp == NULL) {
	printf("failed to open traceStrings.java\n");
	exit(-1);
    }

    fprintf(fp, "package kmonPkg;\n\n");
    fprintf(fp, "public class traceStrings{\n");

    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	fprintf(fp, "public static final int TRACE_%s_MAJOR_ID=%ld;\n",
		traceUnified[i].upName, i);
    }
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	for (j=0;j<traceUnified[i].enumMax;j++) {
	    fprintf(fp, "public static final int %s=%ld;\n",
		    traceUnified[i].traceEventParse[j].eventString, j);
	}
    }

    fprintf(fp, "public static final int numbMajors=%d;\n",
    	    TRACE_LAST_MAJOR_ID_CHECK);

    fprintf(fp, "public static final String majorNames[] = {\n");
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	if (i!=0) fprintf(fp, ", ");
	fprintf(fp, "\"%s\"",traceUnified[i].name);
    }
    fprintf(fp, "};\n");

    fprintf(fp, "public static final int numbMinors[] = {");
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	if (i!=0) fprintf(fp, ", ");
	fprintf(fp, "%ld",traceUnified[i].enumMax);
    }
    fprintf(fp, "};\n");


    // print out minor names
    fprintf(fp, "public static final String minorNames[][] = {\n");
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	if (i!=0) fprintf(fp, ",\n");
	fprintf(fp, "{");
	if (traceUnified[i].enumMax == 0) {
	    fprintf(fp, "\"NOT AN EVENT\"");
	}
	for (j=0;j<traceUnified[i].enumMax;j++) {

	    if (j!=0) fprintf(fp, ", ");
	    fprintf(fp, "\"%s\"",traceUnified[i].traceEventParse[j].eventString);

	}
	fprintf(fp, "}");

    }
    fprintf(fp, "};\n");

    // print out event parse strings
    fprintf(fp, "public static final String eventParse[][] = {\n");
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	if (i!=0) fprintf(fp, ",\n");
	fprintf(fp, "{");
	if (traceUnified[i].enumMax == 0) {
	    fprintf(fp, "\"NOT AN EVENT\"");
	}
	for (j=0;j<traceUnified[i].enumMax;j++) {

	    if (j!=0) fprintf(fp, ", ");
	    fprintf(fp, "\"%s\"",traceUnified[i].traceEventParse[j].eventParse);

	}
	fprintf(fp, "}");

    }
    fprintf(fp, "};\n");

    // print out main print strings
    fprintf(fp, "public static final String eventMainPrint[][] = {\n");
    for (i=0;i<TRACE_LAST_MAJOR_ID_CHECK;i++) {
	if (i!=0) fprintf(fp, ",\n");
	fprintf(fp, "{");
	if (traceUnified[i].enumMax == 0) {
	    fprintf(fp, "\"NOT AN EVENT\"");
	}
	for (j=0;j<traceUnified[i].enumMax;j++) {

	    if (j!=0) fprintf(fp, ", ");
	    fprintf(fp, "\"%s\"",traceUnified[i].traceEventParse[j].eventMainPrint);

	}
	fprintf(fp, "}");

    }
    fprintf(fp, "};\n");

    fprintf(fp, "};\n");

    fclose(fp);

    return 0;
}

