#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenPEMAPI.py,v 1.36 2006/04/13 20:12:29 steve Exp $
#
# Script to generate PEM API
# 
# @author Calin Cascaval
# @date   2004

import string, re, os, sys
import pemEvent, pemGlobals, pemTypes

# -----------------------------------------------------------------------
# main entry point for generating PEM API conformant header files
# -----------------------------------------------------------------------
def genAllClasses(allEvents, allClassIds, outputDir):
    for layer in allEvents.keys():
        genLayerClassDefs(layer, allClassIds, outputDir)
        genFiles(layer, allEvents[layer], outputDir)

    # genEventSwitch(allEvents, allClassIds)

# -----------------------------------------------------------------------
# generate the definitions for the layer classes, i.e.,
# the enum for class ids and the classes masks
# -----------------------------------------------------------------------
def genLayerClassDefs(layer, allClassIds, outputDir):

    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(outputDir, "trace" + layer + "Classes.h"), 'w')
        sys.stdout = fd

    print "#ifndef __TRACE_" + string.upper(layer) + "_CLASSES_H_"
    print "#define __TRACE_" + string.upper(layer) + "_CLASSES_H_"
    genPreamble()

    # another hack for the K42 hw event list
    if layer == 'HW':
        if pemGlobals.hasHWEventList > 0:
            print '#include "traceHWEventList.h"\n'
            
    print '#define', pemEvent.getLayerIdMacro(layer),  
    print pemEvent.PEMLayers[layer]
        
    # generate the major_id enum
    if allClassIds.has_key(layer):
        print 'enum {'
        for classId, classVal in allClassIds[layer]:
            print "\t"+ pemEvent.getEventMajorIdMacro(classId), "=", \
                  classVal, ","
        # the most awful hack until I talk to Bob about the LAST_MAJOR_ID
        if layer == 'OS':
            print "\tTRACE_LAST_MAJOR_ID_CHECK = 26,"
        print "\tTRACE_LAST_" + string.upper(layer) + "_MAJOR_ID_CHECK"
        print '};'
        print '\n'

        # generate the masks
        for classId, classVal in allClassIds[layer]:
            print "#define " + pemEvent.getEventMaskMacro(classId),
            print "(1 <<", pemEvent.getEventMajorIdMacro(classId) +")"
        print '\n'
        
    print "#endif /* #ifndef __TRACE_" + string.upper(layer) + "_CLASSES_H_ */"

    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()
    
# -----------------------------------------------------------------------
# generates header files for each class of events
# -----------------------------------------------------------------------
def genFiles(layer, layerEvents, outputDir):

    fd = None
    
    for classId in layerEvents.keys():
        if pemGlobals.dummyOutput == 0:
            fd = open(os.path.join(outputDir, "trace" + classId + ".h"), 'w')
            sys.stdout = fd

        classIdUpper = string.upper(classId)
        print "#ifndef __TRACE_" + classIdUpper + "_H_"
        print "#define __TRACE_" + classIdUpper + "_H_"
        genPreamble()

        if pemGlobals.genTraceStubs > 0:
            print "#ifdef TRACE_STUBS"
            print "#include <stdio.h>"
            print "#endif"
            print "\n"

        if pemGlobals.dialect == 'AIX':
            print "/* following for AIX trace hooks */"
            # print "#include <sys/trcctl.h>"    ...not sure this is necessary 
            print "#include <sys/trcmacros.h>"
            print "#include <sys/trchkid.h>"
            print "\n"

        if pemGlobals.dialect != 'K42':
            print "#include <traceHeader.h>"
            print "#include <traceRecord.h>"
            print "#include <notify.h>"
        print "#include \"trace" + layer + "Classes.h\"\n"

        print "/* the XLC compiler does not accept inline and long long at the same time."
        print " * Using the _XLC_INLINE macro we can force the inline. */"
        print "#ifdef __GNUC__"
        print "#define _XLC_INLINE static inline"
        print "#else"
        print "#define _XLC_INLINE"
        print "#endif"
        print "\n"
        
        # generate the specifiers enum
        print 'enum {',
        for anEvent in layerEvents[classId]:
            print '\t', pemEvent.getEventSpecifierMacro(anEvent), ','
        # generate the specifier for the interval events
        #for anEvent in layerEvents[classId]:
        #    if anEvent.isIntervalEvent() and anEvent.getIntervalType()=='END':
        #        print '\t', pemEvent.getEventSpecifierMacro(anEvent.transferClassName()), ','
        print '\t',"TRACE_" + string.upper(layer) + "_" + classIdUpper + "_MAX"
        print '};\n'

        # generate global eventId macros
        if pemGlobals.dialect != 'K42':
            for anEvent in layerEvents[classId]:
                print "#define TRACE_"+anEvent.getQualifiedSpecifier()+ \
                      " \\\n"+ "  (" +\
                      "(" + pemEvent.getLayerIdMacro(layer) + "<<20)|(" + \
                      pemEvent.getEventMajorIdMacro(classId) + "<<14)|(" + \
                      pemEvent.getEventSpecifierMacro(anEvent) + "))"

            print '\n'

        if pemGlobals.dialect == 'K42':
            genParseEventStructures(layerEvents[classId], classId)

        # generate the function calls
        if classId != 'Default':
            genInlineMethods(layerEvents[classId], classId)

        print "#endif /* #ifndef __TRACE_" + classIdUpper + "_H_ */"
              
        if pemGlobals.dummyOutput == 0:
            sys.stdout = sys.__stdout__
            fd.close()


# --------------------------------------------------------------------------
# for each event, generate an inline function call that packs the arguments
# into a notifyEvent PEM API call
# --------------------------------------------------------------------------
def genInlineMethods(events, classId):

    # generate the generic inlines
    if pemGlobals.dialect == 'K42':
        print '_XLC_INLINE uval'
        print "trace" + classId + "Enabled() {"
        if pemGlobals.genTraceStubs > 0:
            print "#ifdef TRACE_STUBS"
            print "\tfprintf(stderr, \"trace" + classId + "Enabled()\\n\");"
            print "#else"
        print "\treturn (kernelInfoLocal.traceInfo.mask & " + \
         	pemEvent.getEventMaskMacro(classId) + ");"
        if pemGlobals.genTraceStubs > 0: print "#endif"
        print "}"
        print '\n'

    for anEvent in events:

        print '_XLC_INLINE void'
        print "Trace" + anEvent.getQualifiedSpecifier(),
        print "(",
        # print "TimestampT timestamp",

        hasListFields = 0
        printComma = 0
        for field in anEvent._fieldSeq:
            fieldType = anEvent.getFieldType(field)
            if printComma == 1: print ",",
            else:               printComma = 1
            if fieldType == 'string': print "const",
            if anEvent.isListField(field):
                print pemTypes.getCType(fieldType), '*', field,
                hasListFields = 1
            else:
                print pemTypes.getCType(fieldType), field,
        print ") {"

        if pemGlobals.genTraceStubs > 0:
            print "#ifdef TRACE_STUBS"
            genPrintStatement(anEvent)
            print "#else"

        #if pemGlobals.dialect == 'K42':
        #    genK42MethodBody(anEvent, hasListFields)
        #else:
        #    genAPIMethodBody(anEvent, hasListFields)
        if pemGlobals.dialect == 'AIX':
            genAIXMethodBody(anEvent, hasListFields)
        else:
            genMethodBody(anEvent, hasListFields)
        
        if pemGlobals.genTraceStubs > 0: print "#endif"
        print "}\n"


# ------------------------------------------------------------------------
# generates a method body using the K42 kernel and tracing data structures
# ------------------------------------------------------------------------
def genMethodBody(anEvent, hasListFields):            

    args = anEvent.packFields()
    sortedKeys = args.keys()
    sortedKeys.sort()
    stringsCount = anEvent.countStrings()
        
    if pemGlobals.dialect == 'K42':
        print "\tif(unlikely(kernelInfoLocal.traceInfo.mask & " + \
              pemEvent.getEventMaskMacro(anEvent.getClassId()) + ")) {"
        funcNamePrefix = "\ttraceDefault"
        castUint64 = 'uval64'
    else:
        funcNamePrefix = "notifyEvent"
        castUint64 = 'unsigned long long'
    
    if hasListFields:
        # take the args and the list and pack it into an array of bytes
        print '\t\tunsigned char ___tmpBuffer[1024];'
        print '\t\tunsigned int ___listLength = 0;'
        if stringsCount > 0: print '\t\tunsigned int i;'
        currentKey = 0          # where are we with the other fields
        firstPacked = 0         # how many args are packed in this arg
        for f in anEvent.getTraceFields():
            if anEvent.isListField(f):
                fieldSize = pemTypes.getTypeRawSize(anEvent.getFieldType(f),
                                                    f+"[i]")
                memcpy = '\t\tmemcpy(&___tmpBuffer[___listLength],'

                if anEvent.getFieldType(f) == 'string':
                    print '\t\tfor(i = 0; i <', anEvent.getListCount(f), \
                          '; i++) {'
                    print '\t', memcpy, f + "[i]", ",", fieldSize, ');'
                    print '\t\t\t ___listLength +=', fieldSize, ';\n\t\t}'
                else:
                    fieldSize = fieldSize + '*' + anEvent.getListCount(f)
                    print memcpy, '(char *)', f, ",", fieldSize, ');'
                    print '\t\t___listLength +=', fieldSize, ';'
            else:
                fieldSize = pemTypes.getTypeRawSize(anEvent.getFieldType(f),f)
                if args[currentKey].count(f) == 1:
                    if firstPacked == 0:
                        # argument found as packed
                        print '\t\t*(', castUint64,\
                              '*)&___tmpBuffer[___listLength] =',\
                              args[currentKey], ';'
                        print '\t\t___listLength += 8;'
                        firstPacked = 1
                        byteSize = 0
                    byteSize = byteSize + int(fieldSize)
                    if byteSize == 8:
                        currentKey = currentKey + 1
                        firstPacked = 0
                else:
                    # argument has not been packed
                    print '\t\tmemcpy(&___tmpBuffer[___listLength],',
                    if anEvent.getFieldType(f) != 'string': print '(char *)&',
                    print f, ',', fieldSize, ');'
                    print '\t\t___listLength +=', fieldSize, ';'
                
        # print "\t"+ funcNamePrefix +"Pre"+ str(len(args)-2) +"Bytes (",
        print "\t"+ funcNamePrefix +"Bytes (",
        genNotifyEventId(anEvent)
        #for arg in sortedKeys:
        #    print ",\n\t\t\t" + args[arg],
        #print ");"
        print ",\n\t\t\t___listLength, ___tmpBuffer);"
    elif len(args) < 8 and stringsCount == 0:
        print "\t" + funcNamePrefix + str(len(args)) + "(",
        genNotifyEventId(anEvent)
        for arg in sortedKeys:
            print ",\n\t\t\t" + args[arg],
        print ");"
    else:
        print "\t" + funcNamePrefix + "Generic(",
        genNotifyEventId(anEvent)
        print ",\n\t\t\t",str(len(args)-stringsCount),"/* word(s) */,",
        print str(stringsCount), "/* string(s) */",
        for arg in sortedKeys:
            print ",\n\t\t\t" + args[arg],
        print ");"

    if pemGlobals.dialect == 'K42':
        print "\t}"

# ------------------------------------------------------------------------
# generates a method body using the AIX macros tracing data structures
# ------------------------------------------------------------------------
def genAIXMethodBody(anEvent, hasListFields):

    args = anEvent.packFields()
    sortedKeys = args.keys()
    sortedKeys.sort()
    stringsCount = anEvent.countStrings()

    # assume pemGlobals.dialect == 'AIX':
    print "\tif TRC_ISON(0) {"
    funcNamePrefix = "\tTRCGENT"
    castUint64 = 'unsigned long long'

    # for now treat all events as with ListFields, fill a temp buffer
    # with PEM event and write out as AIX generic event.
    # later change to use AIX macros for words 0,1,2,3,4 & 5, however
    # these write different bits for 32 vs 64 bit applications

    hasListFields = 1

    if hasListFields:
        # take the args and the list and pack it into an array of bytes
        print '\t\tunsigned char ___tmpBuffer[1024];'
        print '\t\tunsigned int ___listLength = 0;'
        if stringsCount > 0: print '\t\tunsigned int i;'
        currentKey = 0          # where are we with the other fields
        firstPacked = 0         # how many args are packed in this arg
        for f in anEvent.getTraceFields():
            if anEvent.isListField(f):
                fieldSize = pemTypes.getTypeRawSize(anEvent.getFieldType(f),
                                                    f+"[i]")
                memcpy = '\t\tmemcpy(&___tmpBuffer[___listLength],'

                if anEvent.getFieldType(f) == 'string':
                    print '\t\tfor(i = 0; i <', anEvent.getListCount(f), \
                          '; i++) {'
                    print '\t', memcpy, f + "[i]", ",", fieldSize, ');'
                    print '\t\t\t ___listLength +=', fieldSize, ';\n\t\t}'
                else:
                    fieldSize = fieldSize + '*' + anEvent.getListCount(f)
                    print memcpy, '(char *)', f, ",", fieldSize, ');'
                    print '\t\t___listLength +=', fieldSize, ';'
            else:
                fieldSize = pemTypes.getTypeRawSize(anEvent.getFieldType(f),f)
                # if args[currentKey].count(f) == 1:
                if args[currentKey].count(f) == 1 and anEvent.getFieldType(f) != 'string':
                    if firstPacked == 0:
                        # argument found as packed
                        print '\t\t*(', castUint64,\
                              '*)&___tmpBuffer[___listLength] =',\
                              args[currentKey], ';'
                        print '\t\t___listLength += 8;'
                        firstPacked = 1
                        byteSize = 0
                    byteSize = byteSize + int(fieldSize)
                    if byteSize == 8:
                        currentKey = currentKey + 1
                        firstPacked = 0
                else:
                    # argument has not been packed
                    print '\t\tmemcpy(&___tmpBuffer[___listLength],',
                    if anEvent.getFieldType(f) != 'string': print '(char *)&',
                    print f, ',', fieldSize, ');'
                    print '\t\t___listLength +=', fieldSize, ';'

        # print "\t"+ funcNamePrefix +"Pre"+ str(len(args)-2) +"Bytes (",
        # print "\t"+ funcNamePrefix +"Bytes (",

        # arg1 = channel (always 0), arg2 = hookid, 32-bits HHH0SSSS
        # HHH = hooked, here the same for all PEM events
        # SSSS = 16 bit subid or 16 bit hookdata
        print "\t"+ funcNamePrefix +"( 0, 0x02000000, \n\t\t\t",

        # emit code to generate PEM EventId ar arg3, the "data word"
        genNotifyEventId(anEvent)
        print ",\n\t\t\t___listLength, ___tmpBuffer);"

    elif len(args) < 8 and stringsCount == 0:
        print "\t" + funcNamePrefix + str(len(args)) + "(",
        genNotifyEventId(anEvent)
        for arg in sortedKeys:
            print ",\n\t\t\t" + args[arg],
        print ");"
    else:
        print "\t" + funcNamePrefix + "Generic(",
        genNotifyEventId(anEvent)
        print ",\n\t\t\t",str(len(args)-stringsCount),"/* word(s) */,",
        print str(stringsCount), "/* string(s) */",
        for arg in sortedKeys:
            print ",\n\t\t\t" + args[arg],
        print ");"

    print "\t}"

# -----------------------------------------------------------------------
# generates a method body using notifyEvent from the PEM API
# -----------------------------------------------------------------------
def genNotifyEventId(anEvent):
    if pemGlobals.dialect == 'K42':
        print pemEvent.getEventMajorIdMacro(anEvent.getClassId()),
        print ",\n\t\t\t"+pemEvent.getEventSpecifierMacro(anEvent),
        print ", \n\t\t\t" + pemEvent.getLayerIdMacro(anEvent.getLayerId()),
    elif pemGlobals.dialect == 'AIX':
        print "TRACE_" + anEvent.getQualifiedSpecifier(),
    else:
        print "TRACE_" + anEvent.getQualifiedSpecifier(),

# -----------------------------------------------------------------------
# generates a method body using notifyEvent from the PEM API
# -----------------------------------------------------------------------
def genAPIMethodBody(anEvent, hasListFields):
    
    args = anEvent.packFields()
    sortedKeys = args.keys()
    if pemGlobals.oldPythonVersion == 1:  sortedKeys.sort()
    stringsCount = anEvent.countStrings()

    if len(args) < 6:
        print "\tnotifyEvent" + str(len(args)) + "(",
    else:
        print "\tnotifyEventVar(",
    #print "timestamp,"
    print "TRACE_" + anEvent.getQualifiedSpecifier(),
    for arg in sortedKeys:
        print ",\n\t\t" + args[arg],
    print ");"


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genPrintStatement(anEvent):
    print "\tfprintf(stderr, \"Trace" + anEvent.getQualifiedSpecifier() + ":",
    fieldSeq = []

    if anEvent._printFormat != None:
        fmtPattern = '%(?P<argNo>\d+)'
        def fieldReplace(matchObject):
            fieldNo = int(matchObject.group('argNo'))
            if fieldNo > len(anEvent._fieldSeq)-1:
                raise "Invalid field index " + str(fieldNo) + " for event " + \
                      str(anEvent._specifier)
            fieldSeq.append(anEvent._fieldSeq[fieldNo])
            return anEvent.getFieldFormat(anEvent._fieldSeq[fieldNo])
        print re.sub(fmtPattern, fieldReplace, anEvent._printFormat),

    print "\\n\"",
    for field in fieldSeq: print ", " + field,
    print ");"

# -----------------------------------------------------------------------
# generate the parse event structures
# -----------------------------------------------------------------------
def genParseEventStructures(allEvents, classId):
    parseMacro = "TRACE_" + string.upper(classId) + "_PARSE_DEFINE_IN_C"
    print '#ifdef', parseMacro
    print "TraceEventParse trace" + classId + "EventParse[] = {"

    # define a function to replace the positional arguments with %pos[%fmt]
    pFmt = '%(?P<argNo>\d+)' # identify a field argument
    def pFmt2k42(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = anEvent.getFieldNameByPos(fieldNo)
        fieldFormat = anEvent.getFieldFormat(fieldName)
        traceFieldNo = anEvent.getTraceFields().index(fieldName)
        return '%' + str(traceFieldNo) + '[' + fieldFormat + ']'

    # generate the parse structure for each event
    for anEvent in allEvents:
        print "\t{ __TR(" + pemEvent.getEventSpecifierMacro(anEvent) + "),"
        parseString = "\"" 
        for field in anEvent.getTraceFields(): # anEvent._fieldSeq:
            parseString = parseString + \
                          pemTypes.getTypeBitSize(anEvent.getFieldType(field)) + " "
        parseString = parseString + "\""
        print "\t" + parseString + ","
        if anEvent._printFormat != None:
            print "\t  \""+re.sub(pFmt, pFmt2k42, anEvent._printFormat)+"\"},"
        else:
            print "\t  \"\" },"
    print "};"
    print '#else /* #ifdef', parseMacro, '*/'
    print "extern TraceEventParse trace" + classId + "EventParse[];"
    print '#endif /* #ifdef', parseMacro, '*/'
    print '\n'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genPreamble():
    print '/**'
    print ' * Automatically generated'
    print ' * !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print ' * Event changes should be made in the XML event definition, '
    print ' * code changes in pemGenPEMAPI.py'
    print ' * (C) Copyright IBM Corp. 2004'
    print ' */'
    print ''
    print '#include <string.h>'
    print '#include <assert.h>'
    # print ''
    # print '#include <trace/trace.H>'
    print '\n'




