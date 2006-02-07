#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenC.py,v 1.27 2005/04/11 13:44:43 cascaval Exp $
#
# Generate C classes for XML event descriptions
# 
# @author Calin Cascaval
# @date   2004

import string, re, os, sys
import pemEvent, pemGlobals

# -----------------------------------------------------------------------
# generate the enum for the HW event list
# -----------------------------------------------------------------------
def genHWEventsEnum(hwEventList, outputDir):
    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(outputDir, "traceHWEventList.h"), 'w')
        sys.stdout = fd
    genPreamble()
    print '#ifndef __TRACE_HW_EVENT_LIST_H_'
    print '#define __TRACE_HW_EVENT_LIST_H_'

    print '\n'
    for proc in hwEventList.keys():
        print "enum HWEventList_" + proc + " {"
        for hwEvent, hwEventDescr in hwEventList[proc]:
            print "\t" + proc + "_" + hwEvent + ", \t/* " + \
                  hwEventDescr + " */"
        print "};\n"

    print ''
    
    for proc in hwEventList.keys():
        print "__attribute__((unused)) static char *HWEventList_" + proc + "_Description[] = {"
        for hwEvent, hwEventDescr in hwEventList[proc]:
            print "\t\"" + hwEventDescr + "\", /* " + hwEvent + " */"
        print "};\n"

    print '#endif /* #ifndef __TRACE_HW_EVENT_LIST_H */'
    
    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()
    
# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genPreamble():
    print '/**'
    print ' * Automatically generated'
    print ' * !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print ' * Event changes should be made in the XML event definition, '
    print ' * code changes in genC.py'
    print ' * (C) Copyright IBM Corp. 2004'
    print ' */'
    print ''
    print '#include <string.h>'
    print '#include <assert.h>'
    # print ''
    # print '#include <trace/trace.H>'
    print '\n'




