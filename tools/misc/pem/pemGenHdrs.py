#! /usr/bin/python
# /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenHdrs.py,v 1.38 2005/06/29 17:18:48 pfs Exp $
#
# Main program for XML PEM headers generation
# 
# @author Calin Cascaval
# @date   2004

import os, string, sys, getopt, re, pprint, xml.parsers.expat
import pemGlobals, pemEvent
import pemGenCPP, pemGenC, pemGenFortran
import pemGenJava
import pemGenPE, pemGenPEMAPI, pemTypes

# the current event object
currentEvent = None
# all events is a map that has as key the layer and as values another map
# that has as keys the classId and as value a list of events
allEvents = {}
# a list of events that are grouped together in a PE trace record
peEvents = []
# hack to preserve the ordering of classIds
classIds = {}
classesMap = {}
classesLayer = None
# HW event list
currentHWProcessor = None
hwEventList = {}    # associates a list of processor events to a processor name
procEvents = []     # list of processor events as a list tuples <event, eventDescr>
# Types map
xmlTypesMap = {}    # a map for user defined types -- types get translated at
                    # parsing time, and they need to map to basic types. The
                    # underlying pemEvent objects will handle only basic types.
# trace layout
traceLayout = pemGlobals.traceLayout

# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
def usage():
    print 'NAME'
    print '\t', sys.argv[0], '-- generates event header files\n'
    print 'SYNOPSIS'
    print '\t', sys.argv[0], '[OPTIONS] ... <FILE(s)>\n'
    print 'DESCRIPTION'
    print '\t Generates event header files from the given XML FILEs for the'
    print '\t specified language. If no language is specified, it defaults to Java.'
    print ''
    print '\t-l, --language=(Java | C++ | C |Fortran)'
    print '\t\tselect the language for the header files' 
    print ''
    print '\t-s, --subLanguage=(K42OLD | K42 | PEM)'
    print '\t\tselect the language dialect for the header files.'
    print '\t\tIt applies only to the C language option and defaults to K42old'
    print ''
    print '\t-o, --outputDir=<dir>'
    print '\t\tselect the directory where to generate the header files'
    print ''
    print '\t-p, --peClassName=peClass'
    print '\t\tselect the name of the PE class that is the union of all'
    print '\t\tXML records'
    print ''
    print '\t-P, --pemClass'
    print '\t\tgenerate only the PEM layer definitions for Java'
    print ''
    print '\t-d, --debug=debugLevel'
    print '\t\tspecify the debug level'
    print ''
    print '\t-n, --dummyOutput'
    print '\t\tgenerate the code to stdout rather than in files'
    print ''

# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------
def main():
    global allEvents
    global peEvents

    version = string.split(string.split(sys.version)[0], '.')
    pemGlobals.oldPythonVersion = int(version[0])
    if int(version[0]) <= 1 and int(version[1]) < 5:
        print sys.argv[0], "requires Python version > 1.5"
        sys.exit(1)

    try:
        optlist, args = getopt.getopt(sys.argv[1:], "hl:s:d:p:o:nP", \
                                      ["help", "language=", "subLanguage",
                                       "debug=", 
                                       "peClassName=", "outputDir=",
                                       "dummyOutput", "pemClass"])
    except getopt.GetoptError:
        usage()
        sys.exit(1)

    defaultOutputDirs = {
        'Java': os.path.join('com','ibm','PEM'),
        'C'   : 'c',
        'C++' : 'cpp',
        'Fortran': 'ftn' }

    files = args
    peClassName = None
    outputDir = None
    pemClass = 0
    for o, a in optlist:
        if o in ("-l", "--language"):
            pemGlobals.language = a
        if o in ("-s", "--subLanguage"):
            pemGlobals.dialect = string.upper(a)
        if o in ("-d", "--debug"):
            pemGlobals.debugLevel = a
        if o in ("-p", "--peClassName"):
            peClassName = a
        if o in ("-o", "--outputDir"):
            outputDir = a
        if o in ("-n", "--dummyOutput"):
            pemGlobals.dummyOutput = 1
        if o in ("-P", "--pemClass"):
            pemClass = 1
        if o in ("-h", "--help"):
            usage()
            sys.exit(0)

    if not defaultOutputDirs.has_key(pemGlobals.language):
        raise "Invalid language " + pemGlobals.language + \
              "\nValid options are: " + str(defaultOutputDirs.keys())
    
    if outputDir == None:
        outputDir = defaultOutputDirs[pemGlobals.language]

    if pemGlobals.language == 'Java' and pemClass == 1:
        pemGenJava.genPEMClass(outputDir)
        sys.exit(0) # we are done, we just wanted the layers

    if files == None or len(files) == 0:
        usage()
        sys.exit(1)

    for fileName in files:
        print '----------------------- Parsing', fileName
        xmlParser = xml.parsers.expat.ParserCreate()
        xmlParser.StartElementHandler = startElementHandler
        xmlParser.CharacterDataHandler = charDataHandler
        xmlParser.EndElementHandler = endElementHandler

        fd = open(fileName, 'r')
        xmlParser.ParseFile(fd)
        fd.close()

    if peClassName != None:
        pemGenPE.genPEClass(peClassName, peEvents)
        sys.exit(0)

    if pemGlobals.debugLevel > 1:
        pp = pprint.PrettyPrinter(indent=4)
        pp.pprint(allEvents)
        sys.exit(0)

    global classIds
    # if no classList tags are found, automatically generate class ids
    if len(classIds) == 0:
        classNo = 0
        for layer in allEvents.keys():
            classIds[layer] = []
            for classId in allEvents[layer].keys():
                classIds[layer].append((classId, classNo))
                classNo = classNo + 1
                                       

    # now generate the code for the specific language
    if pemGlobals.language == 'Java':
        pemGenJava.genAllClasses(allEvents, classIds, outputDir)
        if pemGlobals.hasHWEventList > 0:
            pemGenJava.genHWEventsClass(hwEventList, outputDir)
    elif pemGlobals.language == 'C++':
        pemGenCPP.genAllClasses(allEvents, classIds, outputDir)
        if pemGlobals.hasHWEventList > 0:
            pemGenC.genHWEventsEnum(hwEventList, outputDir)
    elif pemGlobals.language == 'C':
        if pemGlobals.dialect == 'K42OLD':
            #pemGenC.genAllClasses(allEvents, classIds, outputDir)
            raise 'The K42OLD dialect is obsolete'
        else:
            pemGenPEMAPI.genAllClasses(allEvents, classIds, outputDir)
        if pemGlobals.hasHWEventList > 0:
            pemGenC.genHWEventsEnum(hwEventList, outputDir)
    elif pemGlobals.language == 'Fortran':
        pemGenFortran.genAllClasses(allEvents, classIds, outputDir)
    else:
        raise 'Invalid language', pemGlobals.language

# ----------------------------------------------------------------------------
# XML handlers
# ----------------------------------------------------------------------------
def startElementHandler(name, attrs):
    global currentEvent
    global classesLayer, classesMap, classIds
    global traceLayout

    if pemGlobals.debugLevel > 2:
        print 'startElementHandler(', name, ',', attrs, ')'

    if name == "event":
        currentEvent = pemEvent.event()
        currentEvent._name = attrs['name']
        currentEvent._traceLayout = traceLayout
        if attrs.has_key('description'):
            currentEvent._description = attrs['description']
        if attrs.has_key('AIXID'):
            currentEvent._aixId = attrs['AIXID']
        if attrs.has_key('traceLayout'): # overwrite the global setting
            currentEvent._traceLayout = string.lower(attrs['traceLayout'])

    elif name == "field":
        if attrs.has_key('description'): descr = attrs['description']
        else: descr = None
        if attrs.has_key('size'): listSize = attrs['size']
        else: listSize = None
        if not attrs.has_key('type'):
            raise xml.parsers.expat.error('type attribute is mandatory for fields')
        if attrs['type'] == "list":
            currentEvent.addListField(attrs['name'], 'list',
                                      translateUserType(attrs['eltType']),
                                      listSize, descr)
        else:
            currentEvent.addField(attrs['name'],
                                  translateUserType(attrs['type']),
                                  descr)
        if attrs.has_key('vtype'):
            currentEvent._fieldsVisSize[attrs['name']] = attrs['vtype']
        if attrs.has_key('AIXOffset'):
            currentEvent.setAIXOffset(attrs['name'], attrs['AIXOffset'])
        if attrs.has_key('format'):
            currentEvent.setFieldFormat(attrs['name'], attrs['format'])

    elif name == "layerId" and attrs.has_key('value'):
        if not pemEvent.PEMLayers.has_key(attrs['value']):
            raise xml.parsers.expat.error("Invalid LayerId " + attrs['value'])
        currentEvent._layerId = attrs['value']

    elif name == "classId" and attrs.has_key('value'):
        currentEvent._classId = attrs['value']

    elif name == "specifier" and attrs.has_key('value'):
        currentEvent._specifier = attrs['value']

    elif name == "printfString":
        currentEvent._recPrintData = 1

    elif name == "javaFormat":
        raise xml.parsers.expat.error("javaFormat tag is obsolete")

    elif name == "k42Format":
        if pemGlobals.debugLevel > 0:
            print "k42Format tag soon will become obsolete"
        currentEvent._printFormat = convertK42Format(attrs['value'])

    elif name == "printFormat":
        currentEvent._printFormat = attrs['value']

    elif name == "stateChange":
        currentEvent._stateChange = attrs['type'], attrs['value']

    elif name == "sourceCodeInfo":
	if attrs.has_key('pc'):
            currentEvent._sourceCode = attrs['type'], attrs['pc']
        else:
            currentEvent._sourceCode=attrs['type'],attrs['file'],attrs['line']
        # currentEvent._sourceCode = attrs['type'], attrs['file'], attrs['line']

    elif name == "computedField":
        if attrs.has_key('description'): descr = attrs['description']
        else: descr = None
        currentEvent.addComputedField(attrs['name'],
                                      translateUserType(attrs['type']), 
                                      attrs['expr'], descr)

    elif name == "inflatedField":
        if attrs.has_key('description'): descr = attrs['description']
        else: descr = None
	if attrs.has_key('value'): value = attrs['value']
	else: value = None
        if attrs.has_key('size'): listSize = attrs['size']
        else: listSize = None
        if not attrs.has_key('type'):
            raise xml.parsers.expat.error('type attribute is mandatory for fields')
        if attrs['type'] == "list":
	    currentEvent.addInflatedListField(attrs['name'], 'list',
                                    	      translateUserType(attrs['eltType']),
                                      	      listSize, descr)
	else: 
            currentEvent.addInflatedField(attrs['name'],
                                          translateUserType(attrs['type']), 
                                          value, descr)

    elif name == "interval":
	if attrs.has_key('fields'): fields = attrs['fields']
	else: fields = None
	if attrs.has_key('match' ): match  = attrs['match']
	else: match  = None
        currentEvent.addInterval(attrs['name'], attrs['type'], attrs['pair'], match, fields)

    elif name == 'classList':
        classesLayer = attrs['layerId']
	if not classesMap.has_key(classesLayer):
        	classesMap[classesLayer] = {}
       	classIds[classesLayer] = []

    elif name == "class":
        if classesLayer == None: raise "missing layerId in <classList>"
        classesMap[classesLayer][attrs['value']] = attrs['id']

    elif name == 'hwEventList':
        global currentHWProcessor
        currentHWProcessor = attrs['processor']
        pemGlobals.hasHWEventList = 1
    elif name == 'hwEvent':
        global procEvents
        procEvents.append((attrs['value'], attrs['description']))

    elif name == 'map':
        global xmlTypesMap
        if not pemTypes.isBasicPemType(attrs['to']):
            raise xml.parsers.expat.error('Invalid user type map ' + \
                                          attrs['type'])
        xmlTypesMap[attrs['type']] = attrs['to']

    elif name == 'PEM_Events':
        if attrs.has_key('traceLayout'): traceLayout = attrs['traceLayout']

    elif name == 'property':
	# mechanism to pass values between trace records
	field = None
	name = None
        if attrs.has_key('name'):  field = attrs['name']
        if attrs.has_key('field'): name = attrs['field']
	currentEvent.addProperty(name, field)
	

# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
def charDataHandler(data):
    global currentEvent
    if currentEvent != None and currentEvent._recPrintData == 1:
        currentEvent._printfString = data
        currentEvent._recPrintData = 0

# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
def endElementHandler(name):
    global currentEvent, allEvents
    
    if name == "event":
        if not allEvents.has_key(currentEvent.getLayerId()):
            allEvents[currentEvent.getLayerId()] = {}
        if not allEvents[currentEvent.getLayerId()].\
               has_key(currentEvent.getClassId()):
            allEvents[currentEvent.getLayerId()][currentEvent.getClassId()]=[]
        allEvents[currentEvent.getLayerId()][currentEvent.getClassId()].\
             append(currentEvent)
        if pemGenPE.k42FilterEvents.count(currentEvent._specifier) > 0:
            peEvents.append(currentEvent)
            if pemGlobals.debugLevel > 0:
                print 'appending ', currentEvent.getSpecifier()

    elif name == "classList":
        global classIds, classesMap, classesLayer
        if classesLayer == None: raise "missing layerId in <classList>"
        classKeys = classesMap[classesLayer].keys()
        def sortInts(s1, s2):
            return int(s1) - int(s2)
        classKeys.sort(sortInts)
        if pemGlobals.debugLevel > 5: print classKeys
        for aClass in classKeys:
            classIds[classesLayer].append((classesMap[classesLayer][aClass], aClass))
        classesLayer = None

    elif name == 'hwEventList':
        global currentHWProcessor, procEvents, hwEventList
        hwEventList[currentHWProcessor] = procEvents
        procEvents = []

# ----------------------------------------------------------------------------
# converts the k42 format to the new printFormat, by eliminating the print
# format specific for each element
# ----------------------------------------------------------------------------
def convertK42Format(k42Format):
    global currentEvent

    fmtPattern = '%(?P<argNo>\d+)\[%(?P<fmt>\w+)\]'
    def fieldReplace(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = currentEvent._fieldSeq[fieldNo]
        currentEvent._fieldFormat[fieldName] = '%' + matchObject.group('fmt')
        return '%' + str(fieldNo)

    return re.sub(fmtPattern, fieldReplace, k42Format)


# ----------------------------------------------------------------------------
# translate a user defined type to one of the basic types
# ----------------------------------------------------------------------------
def translateUserType(xmlType):
    global xmlTypesMap
    if not pemTypes.isBasicPemType(xmlType):
        if xmlTypesMap.has_key(xmlType):
            return xmlTypesMap[xmlType]
        else:
            raise xml.parsers.expat.error('Non-mapped user type: ' + xmlType)
        
    return xmlType

# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
main()

    
