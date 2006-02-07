#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenFortran.py,v 1.18 2005/06/04 01:00:42 pfs Exp $
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
    print '#include <trace/traceK42.h>'
    print '\n'

def genCWrappers(layerId, classId, layerEvents):

    wrappers = []
    classIdUpper = string.upper(classId)
    genCPreamble()
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

