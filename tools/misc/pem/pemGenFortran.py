#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenFortran.py,v 1.24 2006/04/20 13:58:32 steve Exp $
#
# Generate Fortran++ classes for XML event descriptions
# 
# @author Calin Cascaval
# @date   2004

import string, re, os, sys
import pemEvent, pemGlobals, pemTypes

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genAllClasses(allEvents, allClassIds, outputDir):

    for layer in allEvents.keys():
        # generate all the header files for this layer
        genFiles(layer, allEvents[layer], outputDir)

    # genEventSwitch(allEvents, allClassIds)

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genFiles(layer, layerEvents, outputDir):

    fd = None
    
    for classId in layerEvents.keys():

        # generate first the c wrappers and collect the function file names
        if pemGlobals.dummyOutput == 0:
            fd = open(os.path.join(outputDir, "trace" + classId + ".c"), 'w')
            sys.stdout = fd
        
        if pemGlobals.dialect == 'AIX':
            wrappers = genAixCWrappers(layer, classId, layerEvents)
        else:
            wrappers = genCWrappers(layer, classId, layerEvents)

        if pemGlobals.dummyOutput == 0:
            sys.stdout = sys.__stdout__
            fd.close()

            fd = open(os.path.join(outputDir, "trace" + classId + ".fh"), 'w')
            sys.stdout = fd

        genFortranHeader(layer, classId, layerEvents, wrappers)

        if pemGlobals.dummyOutput == 0:
            sys.stdout = sys.__stdout__
            fd.close()

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genFtnPreamble():
    print '!'
    print '!      Automatically generated'
    print '!      !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print '!      Event changes should be made in the XML event definition, '
    print '!      code changes in genFortran.py'
    print '!      (C) Copyright IBM Corp. 2004'
    print '!'
    print ''
    print '\n'


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genCPreamble():
    print '/**'
    print ' * Automatically generated'
    print ' * !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print ' * Event changes should be made in the XML event definition, '
    print ' * code changes in genFortran.py'
    print ' * (C) Copyright IBM Corp. 2004'
    print ' */'
    print ''
    if pemGlobals.dialect == 'K42':
        print '#include <trace/traceK42.h>'
    print '\n'

def genCWrappers(layerId, classId, layerEvents):

    genCPreamble()

    if pemGlobals.dialect == 'K42':
        wrappers = genK42Wrappers(layerId, classId, layerEvents)
    else:
        wrappers = genGenericWrappers(layerId, classId, layerEvents)
    
    return wrappers

def genK42Wrappers(layerId, classId, layerEvents):

    wrappers = []
    classIdUpper = string.upper(classId)
    print "#include <trace/trace" + classId + ".h>\n"
        
    # generate the trace event macros/inlines
    fName = "trace" + classId + "Enabled"
    wrappers.append(fName)
    fName = string.lower(fName)	
    print 'inline uval'
    print fName + "() {"
    print "\treturn (kernelInfoLocal.traceInfo.mask & " + \
          pemEvent.getEventMaskMacro(classId) + ");"
    print "}"
    print '\n'
    
    # generate the generic inline functions
    # genGenericInlines(classId) 

    # a list of lists of data arguments
    dataArgs = []
    for anEvent in layerEvents[classId]:
        args = []
        for field in anEvent._fieldSeq:
            args.append(anEvent.getFieldType(field))
        if dataArgs.count(args) == 0:
            dataArgs.append(args)

    # generate all the inlines
    for args in dataArgs:
        (words, strings) = countWordsAndStrings(args)

        print 'inline void'
        fName = getMethodName(classId, args)
        wrappers.append(fName)
	fName = string.lower(fName)
        print fName + "(uval16 * minorId",
        whichArg = 0
        for arg in args:
            if arg == 'string':
                print ", "+ pemTypes.getCType(arg) + " data" + str(whichArg),
                print ", int len" + str(whichArg),
            else:
                print ", "+ pemTypes.getCType(arg) +" * data"+ str(whichArg),
            whichArg = whichArg+1
        print ") {"
        print "\t if(unlikely(kernelInfoLocal.traceInfo.mask & " + \
              pemEvent.getEventMaskMacro(classId) + ")) {"
        if strings == 0:
            print "\t\ttraceDefault" + str(len(args)),
            print "(TRACE_" + classIdUpper + "_MAJOR_ID, *minorId," + \
                  pemEvent.getLayerIdMacro(layerId),
        else:
            # add a NULL if there are string args
            whichArg = 0
            for arg in args:
                if arg == 'string':
                    print "\t\t((char *)data"+str(whichArg) + ")[len"+str(whichArg) + \
                          "] = '\\0';"
                whichArg = whichArg + 1
            print "\t\ttraceDefaultGeneric (",
            print "TRACE_" + classIdUpper + "_MAJOR_ID, *minorId," + \
                  pemEvent.getLayerIdMacro(layerId) + ",",
            print str(words) + ", " + str(strings),
        whichArg = 0
        for arg in args:
            if arg == 'string':  print ", data" + str(whichArg),
            else:                print ", *data" + str(whichArg),
            whichArg = whichArg + 1
        print ");"
        print "\t}"
        print "}\n"

    return wrappers

def genGenericWrappers(layerId, classId, layerEvents):

    print '#define uval unsigned int'
    print '#define uval16 unsigned short'
    print '#define uval32 unsigned long'
    print '#define uval64 unsigned long long'
    print "#include <trace" + classId + ".h>\n"
        
    wrappers = []
    classIdUpper = string.upper(classId)
    # a list of lists of data arguments
    dataArgs = []
    for anEvent in layerEvents[classId]:
        args = []
        for field in anEvent._fieldSeq:
            args.append(anEvent.getFieldType(field))
        if dataArgs.count(args) == 0:
            dataArgs.append(args)

    # generate all the inlines
    for args in dataArgs:
        (words, strings) = countWordsAndStrings(args)

        print 'inline void'
        fName = getMethodName(classId, args)
        wrappers.append(fName)
	fName = string.lower(fName)
        print fName + "(uval16 * minorId",
        whichArg = 0
        for arg in args:
            if arg == 'string':
                print ", "+ pemTypes.getCType(arg) + " data" + str(whichArg),
                print ", int len" + str(whichArg),
            else:
                print ", "+ pemTypes.getCType(arg) +" * data"+ str(whichArg),
            whichArg = whichArg+1
        print ") {"

        if strings == 0:
            print "\t\tnotifyEvent" + str(len(args)) + "(",
            print "(("+pemEvent.getLayerIdMacro(layerId) + "<<20)|(TRACE_"\
                  + classIdUpper + "_MAJOR_ID<<14)|(*minorId))",
                  
        else:
            print "\t\tnotifyEventGeneric (",
            print "(("+pemEvent.getLayerIdMacro(layerId) + "<<20)|(TRACE_"\
                  + classIdUpper + "_MAJOR_ID<<14)|(*minorId))" + ",",
            print str(words) + ", " + str(strings),
        whichArg = 0
        for arg in args:
            if arg == 'string':  print ", data" + str(whichArg),
            else:                print ", *data" + str(whichArg),
            whichArg = whichArg + 1
        print ");"
        print "}"

        print "#ifdef __GNUC__"
        print "void "+fName+"_() __attribute__ ((weak,alias(\""+fName+"\")));"
        print "#endif"

        print '\n'

    return wrappers

def genAixCWrappers(layerId, classId, layerEvents):

    wrappers = []
    classIdUpper = string.upper(classId)
    genCPreamble()
    print '#include "trace' + classId + '.h"\n'

    for anEvent in layerEvents[classId]:
        args = []
        for field in anEvent._fieldSeq:
            args.append(anEvent.getFieldType(field))

        fName = "Trace" + anEvent.getQualifiedSpecifier()
        wrappers.append(fName)
        fNameLower = string.lower(fName)
        print fNameLower + " ( ",
        whichArg = 0
        printComma = 0
        for arg in args:
            if printComma == 1: print ", ",
            else:               printComma = 1
            if arg == 'string':
                print pemTypes.getCType(arg) + " data" + str(whichArg),
                print ", int len" + str(whichArg),
            else:
                print pemTypes.getCType(arg) +" * data"+ str(whichArg),
            whichArg = whichArg+1
        print ") {"
        # add a NULL if there are string args
        whichArg = 0
        for arg in args:
            if arg == 'string':
                print "\t((char *)data"+str(whichArg) + ")[len"+str(whichArg) + \
                      "] = '\\0';"
            whichArg = whichArg + 1
        # now call the mixed case C function
        print "\t" + fName + " ( ",
        whichArg = 0
        printComma = 0
        for arg in args:
            if printComma == 1: print ", ",
            else:               printComma = 1
            if arg == 'string':
                print "data" + str(whichArg),
            else:
                print "*data"+ str(whichArg),
            whichArg = whichArg+1
        print " );"
        print "}\n"

    return wrappers

def genFortranHeader(layerId, classId, layerEvents, wrappers):

    genFtnPreamble()

    # generate the specifiers enum
    specCnt = 0
    for anEvent in layerEvents[classId]:
        print '\tinteger*2', pemEvent.getEventSpecifierMacro(anEvent)
        print '\tparameter (', pemEvent.getEventSpecifierMacro(anEvent), '=', str(specCnt),')' 
        specCnt = specCnt + 1
    #for anEvent in layerEvents[classId]:
    #    # generate the specifier for the interval event
    #    if anEvent.isIntervalEvent() and anEvent.getIntervalType()=='END':
    #        intervalEvent = anEvent.transferClassName()
    #        print '\tinteger*2', intervalEvent.getSpecifier()
    #        print '\tparameter (', intervalEvent.getSpecifier(), '=', str(specCnt),')'
    #        specCnt = specCnt + 1

    print '\n'
    for aWrapper in wrappers:
        print '\texternal', aWrapper
        
def countWordsAndStrings(args):
    words = 0
    strings = 0
    for arg in args:
        if arg == 'string': strings = strings+1
        else: words = words + 1
    return (words, strings)

def getMethodName(classId, args):
    sig = {} # hashmap for the argument types
    for arg in args:
        if sig.has_key(arg): sig[arg] = sig[arg] + 1
        else:                sig[arg] = 1
    name = "trace" + classId

    # print " ---------------", classId, sig.keys()

    sigKeys = sig.keys()
    sigKeys.sort()
    if len(sig) == 0:
        name = name + '0'
    elif sigKeys == ['uint64']:
        name = name + str(len(args))
    elif sigKeys == ['string','uint64']:
        name = name + str(sig['uint64']) + "_" + str(sig['string'])
    elif sigKeys == ['string'] and sig['string'] == 1:
        name = name + 'Str'
    else:
        for key in sig.keys():
            name = name + "_" + str(sig[key]) + key 
    return name

