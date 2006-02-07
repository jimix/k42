#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenJava.py,v 1.54 2005/08/01 22:32:49 pfs Exp $
#
# Generate Java classes for XML event descriptions
#
# @author Calin Cascaval
# @date   2004

import string, re, os, sys
import pemEvent, pemGlobals, pemTypes

javaTypeTable = { 'string': 'String',
                  'uint8': 'byte',
                  'uint16': 'short',
                  'uint32': 'int',
                  'uint64': 'long' }        

javaEventClasses = []
javaEventSpecifier = 0

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genJavaClass(anEvent, classIds):
    global javaEventClasses, javaEventSpecifier

    # enum substitute to declare the constant for this class of events
    if javaEventClasses.count(anEvent.getClassId()) == 0:
        print 'final public static int', anEvent.getClassId(), '=',\
              classIds[anEvent.getClassId()], ';'
        javaEventClasses.append(anEvent.getClassId())
        javaEventSpecifier = 0 # reset the event specifier

    # enum substitute for the event type
    print 'final public static int', anEvent.getSpecifier(), '=', \
          javaEventSpecifier, ';'
    javaEventSpecifier = javaEventSpecifier + 1
    
    print '/**'
    print '  * class for event', anEvent._name
    print '  * Automatically generated from XML spec. Do not change!'
    print '  */'
    print 'public static class', anEvent.getClassName(), \
          'extends TraceRecord',

    if anEvent.isStateChangeEvent() or \
           anEvent.isSourceCodeEvent() or \
           anEvent.isIntervalEvent():
        print 'implements',

    if anEvent.isStateChangeEvent():
        print 'TraceStateChangeEvent',

    if anEvent.isSourceCodeEvent():
        if anEvent.isStateChangeEvent():
            print ',',
        print 'TraceSourceCode',

    if anEvent.isIntervalEvent():
        if anEvent.isStateChangeEvent() or anEvent.isSourceCodeEvent():
            print ',',
        print 'TraceIntervalEvent',

    print '{'

    print ''
    # data members
    for field in anEvent._fieldSeq:
        print '\tprotected', getJavaFieldType(anEvent, field), \
              anEvent.getFieldName(field), ';'
        # print '\t', anEvent._fields

    genStreamConstructor(anEvent)
    
    genJavaRecordConstructor(anEvent)

    genGeneratorConstructor(anEvent)
    
    genWriteMethod(anEvent)
    genToStringMethod(anEvent)

    genGetters(anEvent)

    if anEvent.isStateChangeEvent():
        genStateChangeInterface(anEvent)

    if anEvent.isSourceCodeEvent():
        genSourceCodeInterface(anEvent)

    if anEvent.isIntervalEvent():
        genIntervalInterface(anEvent)

    print '\t// ------------------------ PE stuff ------------------------'
    genDumpFieldsToTraceFile(anEvent)
    genDumpFieldsToMetaFile( anEvent)
    genGetNumberOfFields(anEvent)
    genGetName(anEvent)
    genGetDescription(anEvent)
    genGetDefaultView(anEvent)

    genGetNumberOfComputedFields(anEvent)
    genDumpComputedFieldsToMetaFile( anEvent)

    foundListElement = None
    uniqueId    = ""

    for field in anEvent._fieldSeq:
	if anEvent.isListField(field):
	   countField = anEvent.getListCountInXML(field)
           foundListElement = field
	   # get type of list count field not type of array elements!
	   type = anEvent.getFieldType(countField)
	   if   type == "uint32":
	       uniqueId = uniqueId+'+".'+field+':"+Integer.toString('+anEvent.getListCount(field)+')'
	   elif type == "uint64":
	       uniqueId = uniqueId+'+".'+field+':"+Long.toString('+anEvent.getListCount(field)+')'
#	   elif type == "string":
#	       uniqueId = uniqueId+'+".'+field+':"+'+anEvent.getListCount(field)
           else: 
	     raise "genJavaClass() generate getUniqueId() unknown type "

    if foundListElement != None:
       print '\tpublic String getUniqueId() { return "0x"+Integer.toHexString(getEventId())'+uniqueId+"; }"


    print '};'


    #print '\tpublic static', anEvent.getClassName(), \
    #      string.join(('get', anEvent.getClassName()), ''), \
    #      '(TraceInputStream istr) {'
    #print '\t\treturn new', string.join(('HW.',anEvent.getClassName()),''),\
    #      '(istr);'
    #print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genStreamConstructor(anEvent):
    print '\n'
    print '\t/** stream constructor for class readers */'
    print '\tpublic', anEvent.getClassName(), '(TraceInputStream istr)', \
          'throws TraceException, IOException {'
    print '\t\tsuper(istr);'

    # check that the record is of this class
    print '\t\tif(getLayerId() !=', anEvent.getQualifiedLayerId(), \
          '|| getClassId() !=', anEvent.getQualifiedClassId(), \
          '|| getSpecifier() !=', anEvent.getQualifiedSpecifier(),\
          ')'
    print '\t\t\tthrow new TraceException("event " + getLayerId() + ":" +',\
          'getClassId() + ":" + getSpecifier() + " is not of type', anEvent._name,'");'

    if len(anEvent._fieldSeq) > 0:
        print '\t\tfinal TraceInputStream recordStr =', \
              'new TraceInputStream(getPayloadAsStream());'
        print '\t\ttry {'
        
    # read the fields off the payload 
    for field in anEvent.getTraceFields():
        if anEvent.isListField(field):
            listLength = getListLength(anEvent, field)
            print '\t\t', anEvent.getFieldName(field), '= new', \
                  getJavaListFieldType(anEvent, field), \
                  '[(int)', listLength, '];'
            print '\t\tfor(int i = 0; i <', listLength, '; i++)'
            print '\t\t\t', anEvent.getFieldName(field), '[i] =', \
                  string.join(('recordStr.read', \
                               getJavaReaderType(anEvent, field), \
                               '()'), ''), \
                               ';'
                
        else:
            print '\t\t', anEvent.getFieldName(field), '=', \
                  string.join(('recordStr.read', \
                               getJavaReaderType(anEvent, field),'()'),''), \
                               ';'
    if len(anEvent._fieldSeq) > 0:
        print '\t\t} catch(java.io.EOFException ex) { } // fields added to event'

    print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genJavaRecordConstructor(anEvent):
    print '\n'
    print '\t/** copy constructor for class readers */'
    print '\tpublic', anEvent.getClassName(), '(TraceRecord r)', \
          'throws TraceException {'
    print '\t\tsuper(r);'

    # check that the record is of this class
    print '\t\tif(getLayerId() !=', anEvent.getQualifiedLayerId(), \
          '|| getClassId() !=', anEvent.getQualifiedClassId(), \
          '|| getSpecifier() !=', anEvent.getQualifiedSpecifier(),\
          ')'
    print '\t\t\tthrow new TraceException("event " + getLayerId() + ":" +',\
          'getClassId() + ":" + getSpecifier() + " is not of type', anEvent._name,'");'

    if len(anEvent._fieldSeq) > 0:
        print '\t\tfinal TraceInputStream recordStr =', \
              'new TraceInputStream(r.getPayloadAsStream());'

        print '\t\ttry {'
    # read the fields off the payload 
    for field in anEvent.getTraceFields():
        if anEvent.isListField(field):
            listLength = getListLength(anEvent, field)
            print '\t\t', anEvent.getFieldName(field), '= new', \
                  getJavaListFieldType(anEvent, field), \
                  '[(int)', listLength, '];'
            print '\t\tfor(int i = 0; i <', listLength, '; i++)'
            print '\t\t\t', anEvent.getFieldName(field), '[i] =', \
                  string.join(('recordStr.read', \
                               getJavaReaderType(anEvent, field), \
                               '()'), ''), \
                               ';'
                
        else:
            print '\t\t', anEvent.getFieldName(field), '=', \
                  string.join(('recordStr.read', \
                               getJavaReaderType(anEvent, field),'()'),''), \
                               ';'

    if len(anEvent._fieldSeq) > 0:
        print '\t\t} catch(java.io.EOFException ex) { // fields added to event'
        print '\t\t} catch (IOException ex) {'
        print '\t\t\t throw new TraceException(ex.toString());'
        print '\t\t}'

    print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genGeneratorConstructor(anEvent):
    print '\n'
    print '\t/** constructor for trace record generators */'
    print '\tpublic', anEvent.getClassName(), '(int timestamp__',

    # generate all arguments
    for field in anEvent._fieldSeq:
        print ',\n\t\t', getJavaFieldType(anEvent, field), \
              string.replace(anEvent.getFieldName(field), '_', '', 1),
    print ') {'
    
    # compute the record size
    recLen = '8'
#    for field in anEvent._fieldSeq:
#        recLen = recLen + '+' + getJavaTypeSize(anEvent, field)
    print '\t\tsuper(timestamp__,', anEvent.getQualifiedLayerId(), ',', \
          anEvent.getQualifiedClassId(), ',', anEvent.getQualifiedSpecifier(),\
          ', (', recLen, '));'
    # compute the record length
    print '\t\tint recLen = 0;'

    # write all the records
    for field in anEvent.getTraceFields():
        if anEvent.isListField(field):
            print '\t\t', anEvent.getFieldName(field), '= new ',\
                  getJavaListFieldType(anEvent, field), \
                  '[(int)', getListLength(anEvent, field), '];'
            print '\t\tfor(int i = 0; i <', getListLength(anEvent, field), \
                  '; i++) {'
            print '\t\t\t',  anEvent.getFieldName(field), '[i] = ',\
                  string.replace(anEvent.getFieldName(field), '_', '', 1),\
                  '[i];'
            print '\t\t\trecLen +=', \
                  pemTypes.getTypeRawSize(anEvent.getFieldType(field),field+"[i]"),';'
            print '\t\t}'
        else:
            print '\t\t',  anEvent.getFieldName(field), ' = ',\
                  string.replace(anEvent.getFieldName(field), '_', '', 1), \
                  ';'
            print '\t\trecLen +=', \
                  pemTypes.getTypeRawSize(anEvent.getFieldType(field), field), ';'
    print '\t\tif ((recLen % 8) != 0) recLen = recLen/8 + 1;'
    print '\t\telse recLen = recLen/8;'
    print '\t\tsetRecordLength(recLen+1);'
    print '\t}'

# -----------------------------------------------------------------------
# serialize into a PEM trace record
# -----------------------------------------------------------------------
def genWriteMethod(anEvent):
    print '\n\t/** serialization */'
    print '\tpublic void write(TraceOutputStream ostr) throws IOException {'
    print '\t\tsuper.write(ostr);'
    bytesWritten = 0         # keep track of the number of bytes written
                             # before a list such that we can pad it to 8!
    # write all the records
    for field in anEvent.getTraceFields():
        if anEvent.isListField(field):
            if bytesWritten % 8 != 0:
                print '\t\tfor(int i =', str(bytesWritten), '; i < 8; i++)'
                print '\t\t\tostr.writeByte(0);'
            print '\t\tfor(int i = 0; i <', getListLength(anEvent, field), \
                  '; i++)'
            print '\t\t\t', string.join(('ostr.write', \
                                         getJavaReaderType(anEvent, field), \
                                         '(', anEvent.getFieldName(field), \
                                         '[i]);'), '')
        else:
            print '\t\t', string.join(('ostr.write', \
                                       getJavaReaderType(anEvent, field), \
                                       '(', anEvent.getFieldName(field), \
                                       ');'), '')
            if anEvent.getFieldType(field) != 'string':
                bytesWritten = bytesWritten + \
                               int(pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                                           field))
                                   
    print '\t}'


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genToStringMethod(anEvent):
    if anEvent._printFormat == None: return

    print '\n'
    print '\tpublic String toString() {'
    print '\t\tString result =',
    print '\"\"+getTimestamp()+" "+getSpecifierAsString()+" "+',
    print "\"" +  printFieldsToString(anEvent) + "\"",
    print ';'
    print '\t\treturn result;'
    print '\t}'

# -----------------------------------------------------------------------
# generate one getter for each field
# -----------------------------------------------------------------------
def printFieldsToString(anEvent):

    if anEvent._printFormat == None: return ""
    
    fmtPattern = '%(?P<argNo>\d+)'
    def fieldReplace(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = anEvent.getFieldNameByPos(fieldNo)
        fieldFormat = anEvent.getFieldFormat(fieldName)
        
        if anEvent.isListField(fieldName):
            result = "\";\n\t\tfor(int i = 0; i < "
            result = result + getListLength(anEvent, fieldName) + "; i++) \n"
            result = result + "\t\t\t"
            if fieldFormat[len(fieldFormat)-1] == 'x':
                result = result + "result += Long.toHexString(get_" + \
                         fieldName + "(i)) + \" \";"
            else:
                result = result + " result += get_"+ fieldName + "(i) + \" \";"
            result = result + "\n\t\tresult += \""
            return result
        else:
            if fieldFormat[len(fieldFormat)-1] == 'x':
                return "\" + Long.toHexString(get_" + fieldName + "()) + \""
            else:
                return "\" + get_" + fieldName + "() + \""

    return re.sub(fmtPattern, fieldReplace, anEvent._printFormat)

# -----------------------------------------------------------------------
# generate one getter for each field
# -----------------------------------------------------------------------
def genGetters(anEvent):
    print '\n\t // --------------- accessors -----------------------------'
    for field in anEvent._fieldSeq:
        print '\tpublic', 
        if anEvent.isListField(field):
            print getJavaListFieldType(anEvent, field),
            print string.join(('get', anEvent.getFieldName(field)), ''), '(',
            print 'int i',
            print ') { return', anEvent.getFieldName(field),
            print '[i]',
        else:
            print getJavaFieldType(anEvent, field),
            print string.join(('get', anEvent.getFieldName(field)), ''), '(',
            print ') { return', anEvent.getFieldName(field),
        print '; }'

    if anEvent._computedFields != None and len(anEvent._computedFields) > 0:
        print '\n\t// ------------- accessors for computed fileds ------------'
        for field in anEvent._computedFields:
            print '\tpublic', getJavaFieldType(anEvent, field),
            print string.join(('get', anEvent.getFieldName(field)), ''), '() {',
            print 'return', anEvent.printAccessorsOfComputedFields(field), ';',
            print '}' 
#           print 'return', printAccessorsOfComputedFields(anEvent.getFieldExpression(field)), ';',

    if anEvent._inflatedFields != None and len(anEvent._inflatedFields) > 0:	
        print '\n\t// ------------- accessors for inflated fields ------------'
        for field in anEvent._inflatedFields:
            print '\tpublic', 
            if anEvent.isListField(field):
                print getJavaListFieldType(anEvent, field),
                print string.join(('get', anEvent.getFieldName(field)), ''), '(',
                print 'int i',
                print ') { return', printPropertyListGetter(anEvent, field, "i"), ';}' 
	    else: 
                print getJavaFieldType(anEvent, field),
                print string.join(('get', anEvent.getFieldName(field)), ''), '() {',
                print 'return', printPropertyGetter(anEvent, field), ';', 
                print '}' 

    print '\n'
    # generate the getSpecifierAsString
    print "\tpublic String getSpecifierAsString() { return \"" + \
          anEvent.getSpecifier()+" 0x\" + Integer.toHexString(getEventId()); }"
#          anEvent.getSpecifier()+"\"; }"
#          anEvent.getSpecifier()+" \" + Integer.toString(getEventId()) + \" (\"+Integer.toHexString(getEventId()) +\

    # generate the getClassName (PFS added)
    print "\tpublic String getClassName() { return \"" + \
          anEvent.getClassName()+"\"; }"


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def printPropertyGetter(anEvent, field ):
    type  = javaTypeTable[anEvent.getFieldType(field)]
    return "InflatedFields.get_"+type+"(\""+field+"\")"

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def printPropertyListGetter(anEvent, field, index ):
    type  = javaTypeTable[anEvent.getFieldType(field)]
    return "InflatedFields.get_"+type+"(\""+field+"\","+index+")"

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def printPropertySetter(anEvent, field):
    value = anEvent.getFieldValue(field)
    type  = javaTypeTable[anEvent.getFieldType(field)]
    return "InflatedField.put_"+type+"("+field+","+value+")"

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getJavaFieldType(anEvent, fieldName, visualizerType = 0):
    global javaTypeTable

    if not anEvent.hasField(fieldName):
        return None
    fieldType = anEvent.getFieldType(fieldName)
    if visualizerType == 1 and anEvent._fieldsVisSize.has_key(fieldName):
        fieldType = anEvent._fieldsVisSize[fieldName]
    if anEvent.isListField(fieldName):
        return string.join((javaTypeTable[fieldType],'[]'))
    else:
        return javaTypeTable[fieldType]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getJavaListFieldType(anEvent, fieldName):
    return javaTypeTable[anEvent.getFieldType(fieldName)]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getJavaReaderType(anEvent, fieldName, visualizerType = 0):
    javaReaderTypeTable = { 'string': 'String',
                            'uint8': 'Byte',
                            'uint16': 'Short',
                            'uint32': 'Int',
                            'uint64': 'Long' }
    fieldType = anEvent.getFieldType(fieldName)
    if visualizerType == 1 and anEvent._fieldsVisSize.has_key(fieldName):
        fieldType = anEvent._fieldsVisSize[fieldName]
    return javaReaderTypeTable[fieldType]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getJavaTypeSize(anEvent, fieldName):
    javaTypeTable = {
        'string': string.join((anEvent.getFieldName(fieldName),
                               '.length()'), ''),
        'uint8': '1',
        'uint16': '2',
        'uint32': '4',
        'uint64': '8'
        }        
    if anEvent.isListField(fieldName):
        return string.join((anEvent.getFieldName(fieldName),
                            '.length * ',
                            javaTypeTable[anEvent.getFieldType(fieldName)]),\
                           '')
    else:
        return javaTypeTable[anEvent.getFieldType(fieldName)]
    

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getListLength(anEvent, field):
    if anEvent.getListCount(field) != None:
        return anEvent.getListCount(field)
    else:
        prevFieldsSize = 0
        for aField in anEvent._fieldSeq:
            if field == aField: break
            prevFieldsSize = prevFieldsSize + int(getJavaTypeSize(anEvent, aField))
        prevFieldsSize = prevFieldsSize / 8
        return "getPayloadLength() - " + str(prevFieldsSize)


# -----------------------------------------------------------------------
#  generate the "switch" class
# -----------------------------------------------------------------------
def genEventSwitch(pemPath, allEvents):
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(pemPath, 'Events', "PEMEvents.java"), 'w')
        sys.stdout = fd

    genPreamble(".Events")
    print 'import com.ibm.PEM.Events.*;'
    print '\n'
    print 'public class PEMEvents {'
    print '\n'
    print '  public static TraceRecord getEvent(TraceRecord r)'
    print '\t\tthrows TraceException, IOException {'
    print '\tswitch(r.getEventId()) {'

    for layer in allEvents.keys():
        for classId in allEvents[layer].keys():
            for anEvent in allEvents[layer][classId]:
                print '\tcase', anEvent.getEventId(), ':'
                print '\t\treturn new', anEvent.getQualifiedClassName(), '(r);'

    print '\tdefault: return r; // unknown'
    print '\t}'
    print '}'

    print '}'

    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()

    

# -----------------------------------------------------------------------
# generate the methods required by the TraceStateChangeEvent interface
# -----------------------------------------------------------------------
def genStateChangeInterface(anEvent):
    print '\n\t// ------------ TraceStateChangeEvent interface ------------'
    print '\tpublic final int getStateChangeType() {'
    print "\t\treturn TraceStateChangeEvent." + anEvent.getStateChangeType() \
          + ";"
    print '\t}'
    print ''
    print '\tpublic final int getState() {'
    print '\t\treturn (int)(', anEvent.getStateChangeNewState(), ' >> 32);'
    print '\t}'
    print ''
    print '\tpublic final long getCommID() {'
    print '\t\treturn ', anEvent.getStateChangeNewState(), ';'
    print '\t}'

# -----------------------------------------------------------------------
# generate the methods required by the TraceSourceCode interface
# -----------------------------------------------------------------------
def genSourceCodeInterface(anEvent):
    print '\n /** TraceSourceCode interface */'
    print '\tpublic final int getSourceCodeEventType() {'
    print "\t\treturn TraceSourceCode." + anEvent.getSourceCodeType() \
          + ";"
    print '\t}'
    print ''
    if anEvent.getSourceCodeType() == 'SOURCE_FILE':
        print '\tpublic final String getSourceCodeLine() {'
        print "\t\treturn \"" + anEvent.getSourceCodeInfo() + "\";"
        print '\t}'
        print ''
        print '\tpublic String getSourceCodeLine(String executablePath) {'
        print '\t\treturn "";'
        print '}'
    else:
        print '\tpublic final String getSourceCodeLine() { return ""; }'
        print ''
        print '\tpublic String getSourceCodeLine(String executablePath) {'
        print '\t\tfinal String cmdLine = "addr2line -e " + executablePath + " " + Long.toHexString(', "get_" + anEvent.getSourceCodePC() + "()", ');'
        print '\t\ttry {'
        print '\t\t\tfinal Process addr2line = Runtime.getRuntime().exec(cmdLine);'
        print '\t\t\taddr2line.waitFor();'
        print '\t\t\tfinal byte addr2lineOutput[] = new byte[1024];'
        print '\t\t\taddr2line.getInputStream().read(addr2lineOutput);'
        print '\t\t\treturn new String(addr2lineOutput);'
        print '\t\t} catch (Exception ex) {'
        print '\t\t\treturn new String("??:??");'
        print '\t\t}'
        print '\t}'
        

def genPreamble(pkg):
    print '/**'
    print ' * Automatically generated'
    print ' * !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print ' * Event changes should be made in the XML event definition, '
    print ' * code changes in genJava.py'
    print ' * (C) Copyright IBM Corp. 2004'
    print ' */'
    print ''
    print "package com.ibm.PEM" + pkg + ";"
    print ''
    if len(pkg) > 0:
        print 'import com.ibm.PEM.PEM;'
    # to access inflated fields    	
    print 'import com.ibm.PEM.*;'
    print 'import com.ibm.PEM.traceFormat.PerformanceExplorer.*;'
    print 'import com.ibm.PEM.traceFormat.*;'
    print 'import java.io.*;'
    print 'import java.text.*;'
	
    print '\n'

def genPEMClass(pemPath):

    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(pemPath, "PEM.java"), 'w')
        sys.stdout = fd

    genPreamble("")
    print 'public class PEM {'
    print ''
    print '  final public static int LITTLE_ENDIAN = 1;'
    print '  final public static int BIG_ENDIAN = 0;'

    print '  final public static int PEM_TRACE_VERSION = ((1 << 16) + (0 << 8) + (0));'

    print '  public final class Layer {'
    for l in pemEvent.PEMLayers.keys():
        print '    final public static int', l, '=', \
              pemEvent.PEMLayers[l], ';'
    print '  };'

    print '};'

    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()

def genAllClasses(allEvents, allClasses, outputDir):
    fd = None
    # hack to preserve the ordering of classIds
    for layer in allEvents.keys():
        if pemGlobals.dummyOutput == 0:
            if not os.path.exists(os.path.join(outputDir, 'Events')):
                os.mkdir(os.path.join(outputDir, 'Events'))
            fd = open(os.path.join(outputDir, 'Events', layer + ".java"), 'w')
            sys.stdout = fd

        genPreamble(".Events")

        classIds = {}
        if allClasses.has_key(layer):
            for classId, classVal in allClasses[layer]:
                classIds[classId] = classVal

        print 'public class', layer, '{'
        for classId in allEvents[layer].keys():
            for anEvent in allEvents[layer][classId]:
                print '\n'
                genJavaClass(anEvent, classIds)
            # generate the interval class that contains the common fields from
            # the start/end interval events and the start/end timestamps
            for anEvent in allEvents[layer][classId]:
                if anEvent.isIntervalEvent() and \
                       anEvent.getIntervalType() == 'END' or \
                       anEvent.getIntervalType() == 'PERIODIC':
#                    print 'genAllClasses('+anEvent._name+') interval\n'
		    pair      = None
                    dummyPair = None
                    if anEvent._intervalEnds != None:
#                        print 'genAllClasses('+anEvent._name+') _intervalEnds != None\n'
                        if anEvent._intervalType == 'START':   
                            dummyPair = anEvent._intervalEnds[1]	# this can never happen
                        else: dummyPair = anEvent._intervalEnds[0]
                    if not dummyPair == None:
#			print 'dummyPair is '+dummyPair._name+', layer '+dummyPair.getLayerId()+'\n'
                        for pair in allEvents[dummyPair.getLayerId()][dummyPair.getClassId()]:
			    if pair._name == dummyPair._name:
				break
		    if dummyPair._name != pair._name:
                        raise "Invalid interval match for " + anEvent._name + "!"

                    genIntervalClass(anEvent, pair, classIds)

        print '};'
        if pemGlobals.dummyOutput == 0:
            sys.stdout = sys.__stdout__
            fd.close()

        # generate the PEM class that contains the layer defs
        genPEMClass(outputDir)

        # generate the class that contains the switch for all the events
        genEventSwitch(outputDir, allEvents)

# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
def genDumpFieldsToTraceFile(anEvent):

    print '\t// dump fields to trace file'
    print '\tpublic final void dumpFieldsToTraceFile(TraceOutputStream traceFile) {'
#    print "\tSystem.out.println(\"" + anEvent.getQualifiedClassName() + \
#          ".dumpFields() \"+toString());"
    # data members
    for field in anEvent.getPETraceFields():
	if anEvent.isListField(field):
            print '\t  for(int i = 0; i <', getListLength(anEvent, field),
            print '; i++)'
            print "\t\ttraceFile.write"+getJavaReaderType(anEvent, field, 1),
            if isAnInflatedField(anEvent, field):
                print "(("+ getJavaListFieldType(anEvent, field) + ")" + \
	                 "get"+anEvent.getFieldName(field) + "(i));"
	    else: 
                print "(("+ getJavaListFieldType(anEvent, field) + ")" + \
                          anEvent.getFieldName(field) + "[i]);"
        else:
            if isAnInflatedField(anEvent, field):
                print "\t  traceFile.write"+getJavaReaderType(anEvent, field, 1)+\
                      "(("+ getJavaFieldType(anEvent, field, 1) + ")" + \
                      "get"+anEvent.getFieldName(field) + "());"
	    else:
                print "\t  traceFile.write"+getJavaReaderType(anEvent, field, 1)+\
                      "(("+ getJavaFieldType(anEvent, field, 1) + ")" + \
                      anEvent.getFieldName(field) + ");"

    #print '\t  System.out.println("***Missing type for field ', \
    #      anEvent.getFieldName(field), '!***");'

    print '\t}'

#----------------------------------
# return true if an inflated field
#---------------------------------
def isAnInflatedField(anEvent, fieldName):
    if 	anEvent._inflatedFields != None and len(anEvent._inflatedFields) > 0:
        for field in anEvent._inflatedFields:
            if field == fieldName:
	         return 1

    return 0

# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
def genDumpFieldsToMetaFile(anEvent):

    print '\t// dump field descriptions to meta file'
    print '\tpublic final void dumpFieldsToMetaFile(PE2MetaFile metaFile) {'
    # print '\t  System.out.println("dumpFieldsToMetaFile(): "+toString());'
    # data members
    for field in anEvent.getPETraceFields():
        if anEvent.isListField(field):
            print '\t for(int i = 0; i <', getListLength(anEvent, field),
            print '; i++)'
            print "\t\tmetaFile.writePFD_" + \
                  getJavaListFieldType(anEvent, field),
            print "(\""+ anEvent.getFieldNameStripped(field) + "\"+i, \"" + \
                  anEvent.getFieldDescription(field) + "\", true);" 
        else:
            print "\t metaFile.writePFD_"+getJavaFieldType(anEvent, field, 1)+\
                  "( \"" + anEvent.getFieldNameStripped(field) + "\", \"" + \
                  anEvent.getFieldDescription(field) + "\", true);"

    #print '\t  System.out.println("***Missing type for field ', \
    #      anEvent.getFieldName(field), '!***");'

    print '\t}'

def genDumpComputedFieldsToMetaFile(anEvent):

    print '\t// dump computed field descriptions to meta file'
    print '\tpublic final void dumpComputedFieldsToMetaFile(PE2MetaFile metaFile) {'
    # print '\t  System.out.println("dumpComputedFieldsToMetaFile(): "+toString());'
    # data members
    for field in anEvent._computedFields:
	# Calin can a list be here?  PFS
        print "\t  metaFile.writeLogicalFieldDefinition" + \
              "( \"" + anEvent.getFieldNameStripped(field) + "\", \"" + \
              anEvent.getFieldDescription(field) + "\", \"" + \
              anEvent.printComputedFields(field) + "\");"
#              printComputedFields(anEvent.getFieldExpression(field)) + "\");"
              

    print '\t}'


# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
def genGetNumberOfFields(anEvent):
    print '\t// meta file\'s number of fields for an event'
    print '\tpublic final int getNumberOfFields() { return',
    allFields = "0"
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field):
            allFields = allFields + "+(int)"+ getListLength(anEvent, field)
        else:
            allFields = allFields + "+1";
    for field in anEvent._inflatedFields:
        if anEvent.isListField(field):
            allFields = allFields + "+(int)"+ getListLength(anEvent, field)
        else:
            allFields = allFields + "+1";
    print allFields, ";}"


# -----------------------------------------------------------------------
# PFS added    
# Generate number of computed fields
# -----------------------------------------------------------------------
def genGetNumberOfComputedFields(anEvent):
    print '\t// meta file\'s number of computed fields for an event'
    print '\tpublic final int getNumberOfComputedFields() { return',
    allFields = "0"
    for field in anEvent._computedFields:
        if anEvent.isListField(field):
            allFields = allFields + "+(int)"+ getListLength(anEvent, field)
        else:
            allFields = allFields + "+1";
    print allFields, ";}"


# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
def genGetName(anEvent):
    print '\t// meta file\'s name for an event'
    print '\tpublic final String getName() {'
    print "\t\treturn \"" + string.replace(anEvent._name, "::", ".") + "\";"
    print '\t}'


# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
def genGetDescription(anEvent):
    print '\t// meta file\'s description for an event'
    print '\tpublic final String getDescription() {'
    if (anEvent.getDescription() == None):
      print '\t\treturn "no description specified!";'
    else:
      print string.join(('\t\treturn "', anEvent.getDescription(), '";'), '')
    print '\t}'

# -----------------------------------------------------------------------
# PFS added    
# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genGetDefaultView(anEvent):
    if anEvent._printFormat == None: return

    fmtPattern = '%(?P<argNo>\d+)'
    def strippedFieldReplace(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = anEvent._fieldSeq[fieldNo]
        fieldFormat = anEvent.getFieldFormat(fieldName)
        
        if anEvent.isListField(fieldName):
            result = "\";\n\t\tfor(int i = 0; i < "
            result = result + getListLength(anEvent, fieldName) + "; i++) \n"
            result = result + "\t\t\t"
            if fieldFormat[len(fieldFormat)-1] == 'x':
                result = result + "result += Long.toHexString(get_" + \
                         fieldName + "(i)) + \" \";"
            else:
                result = result + " result += get_"+ fieldName + "(i) + \" \";"
            result = result + "\n\t\tresult += \""
            return result
        else:
            if fieldFormat[len(fieldFormat)-1] == 'x':
                return "\\\"+ hex(" + fieldName + ")+\\\""
            else:
                return "\\\"+" + fieldName + "+\\\""

    

    print '\n'
    print '\tpublic final String getDefaultView() {'
    print '\t\tString result =',
    print '\"timeStamp+ ',
    print '\\\"'+ re.sub(fmtPattern, strippedFieldReplace, anEvent._printFormat) + '\\\""',
    print ';'
    print '\t\treturn result;'
    print '\t}'
    print '\n'

# -----------------------------------------------------------------------
# generate the TraceIntervalEvent interface
# -----------------------------------------------------------------------
def genIntervalInterface(anEvent):
    print '\n\t // ----------------- TraceIntervalEvent interface --------'

    print '\t/** return whether the event starts or ends an interval */'
    print '\tpublic boolean isInterval()    { return false;  }'
    if anEvent.getIntervalType() == 'START':
        print '\tpublic boolean isIntervalStart()    { return true;  }'
        print '\tpublic boolean isIntervalEnd()      { return false; }'
        print '\tpublic boolean isIntervalPeriodic() { return false; }'

    elif anEvent.getIntervalType() == 'END':
        print '\tpublic boolean isIntervalStart()    { return false; }'
        print '\tpublic boolean isIntervalEnd()      { return true;  }'
        print '\tpublic boolean isIntervalPeriodic() { return false; }'

    elif anEvent.getIntervalType() == 'PERIODIC':
        print '\tpublic boolean isIntervalStart()    { return false; }'
        print '\tpublic boolean isIntervalEnd()      { return false; }'
        print '\tpublic boolean isIntervalPeriodic() { return true;  }'

    else:
	print '/** ***Interval type unknown!*** **/ '	
        print '\tpublic boolean isIntervalStart()    { return false; }'
        print '\tpublic boolean isIntervalEnd()      { return false; }'
        print '\tpublic boolean isIntervalPeriodic() { return false; }'

    print '\t/** returns the pair\'s unique event identifier */'
    pairEvent = anEvent.getIntervalPair()
    print '\tpublic int getPairEventId() { return',pairEvent.getEventId(),'; }'

    print '\t/** matching function */'
    print '\tpublic boolean matches(TraceRecord r) throws TraceException {'
    print '\t\tif(r.getEventId() != getPairEventId())'
    print '\t\t\tthrow new TraceException("Invalid record " + r.getName() + ". Looking for', pairEvent.getQualifiedClassName(), '");'
    print '\t\tfinal', pairEvent.getClassName(), 'pair = (', \
          pairEvent.getClassName(), ')r;'
    print '\t\treturn',
    if len(anEvent._intervalMatch) == 0:
        print 'true',
    else:
        first = 1
        for field in anEvent._intervalMatch:
            if first == 1: first = 0
            else:          print '&&',
            print "(get_" + field + "() == pair.get_" + field + "())",
    print ';'
    print '\t}'

    print '\tpublic String getIntervalClassName() { return',
    intervalEvent = anEvent.transferClassName()
    print "\"" + intervalEvent.getClassName() + "\"; }"
    print "\n"

    genCreateIntervalInstance(anEvent)
    
# -----------------------------------------------------------------------
# Generate an interval class.
# For a matched pair, the common fields are specified in the intervalMatch
# and intervalFields of the start and end interval events and the events timestamps
# For a periodic, the common fields are specified in the periodic interval event. 
# -----------------------------------------------------------------------
def genIntervalClass(endEvent, startEvent, classIds):
    global javaEventSpecifier, javaEventClasses

    anEvent = endEvent.makeIntervalEvent(startEvent)

    # enum substitute to declare the constant for this class of events
    if javaEventClasses.count(anEvent.getClassId()) == 0:
        if not classIds.has_key(anEvent.getClassId()):
            vals = classIds.values()
            vals.sort()
            classIds[anEvent.getClassId()]= int(vals[len(vals)-1])+1
        print 'final public static int', anEvent.getClassId(), '=',\
              classIds[anEvent.getClassId()], ';'
        javaEventClasses.append(anEvent.getClassId())
        javaEventSpecifier = 0 # reset the event specifier

    # enum substitute for the event type
    print '\n\nfinal public static int', anEvent.getSpecifier(), '=', \
          javaEventSpecifier, ';'
    javaEventSpecifier = javaEventSpecifier + 1
    
    print '/** -------------------------------------------------------'
    print '  * class for event', anEvent._name
    print '  * ',anEvent._description
    print '  * Automatically generated from XML spec. Do not change!'
    print '  */'
    print 'public static class', anEvent.getClassName(), \
          'extends TraceRecord',

    print '{'

    print ''
    # data members
    for field in anEvent.getTraceFields():
        print '\tprotected', getJavaFieldType(anEvent, field), \
              anEvent.getFieldName(field), ';'
        # print '\t', anEvent._fields


    genIntervalConstructor(anEvent, startEvent, endEvent)

    print '\tpublic boolean isInterval()    { return true;  }'
    genToStringMethod(anEvent)

    genGetters(anEvent)

    genDumpFieldsToTraceFile(anEvent)
    genDumpFieldsToMetaFile( anEvent)
    genGetNumberOfFields(anEvent)
    genGetName(anEvent)
    genGetDescription(anEvent)
    genGetDefaultView(anEvent)

    genGetNumberOfComputedFields(anEvent)
    genDumpComputedFieldsToMetaFile( anEvent)


    foundListElement = None
    uniqueId    = ""

    for field in anEvent._fieldSeq:
	if anEvent.isListField(field):
	   countField = anEvent.getListCountInXML(field)
           foundListElement = field
	   # get type of list count field not type of array elements!
	   type = anEvent.getFieldType(countField)
	   if   type == "uint32":
	       uniqueId = uniqueId+'+".'+field+':"+Integer.toString('+anEvent.getListCount(field)+')'
	   elif type == "uint64":
	       uniqueId = uniqueId+'+".'+field+':"+Long.toString('+anEvent.getListCount(field)+')'
#	   elif type == "string":
#	       uniqueId = uniqueId+'+".'+field+':"+'+anEvent.getListCount(field)
           else: 
	     raise "genJavaClass() generate getUniqueId() unknown type "

    if foundListElement != None:
       print '\tpublic String getUniqueId() { return "0x"+Integer.toHexString(getEventId())'+uniqueId+"; }"



    print '};'

# --------------------------------------------------------------------------
# generate a constructor for an interval event
# --------------------------------------------------------------------------
def genIntervalConstructor(intervalEvent, startEvent, endEvent):

    print '\t/*\n\t * constructor for interval event '+intervalEvent._name+'\n\t */'
    print '\tpublic', intervalEvent.getClassName(), \
          '(TraceRecord startEvent, TraceRecord endEvent, long startTime, long endTime) throws TraceException {'
#    print '\t\tsuper((int)startTime,', \	# good
    print '\t\tsuper(startEvent.getTimestampInt(),', \
          intervalEvent.getQualifiedLayerId(), ',', \
          intervalEvent.getQualifiedClassId(), ',', \
          intervalEvent.getQualifiedSpecifier(), ', 8);'
    print ''
    print '\t\t// check that the arguments are actually the right interval ends'
    print '\t\tif(startEvent.getEventId() !=', \
          intervalEvent._intervalEnds[0].getEventId(), \
          '|| endEvent.getEventId() !=', \
          intervalEvent._intervalEnds[1].getEventId(), ')'
    print '\t\t\tthrow new TraceException("Invalid interval ends");'
    print ''
    print '\t\t_startTime = startTime;'
    print '\t\t_endTime = endTime;'
    if intervalEvent.getIntervalType() == 'PERIODIC':
        for field in intervalEvent.getTraceFields():
	    if not (intervalEvent.getFieldName(field) == '_startTime' or \
	       intervalEvent.getFieldName(field) == '_endTime'):
                if (intervalEvent.getFieldType(field)) == 'string':
                    print "\t\t" + intervalEvent.getFieldName(field) + " = ((" + \
                          intervalEvent._intervalEnds[1].getQualifiedClassName() + \
                          ")endEvent).get_" + field + "();"
                else:
                    print "\t\t"  + intervalEvent.getFieldName(field) + " = " +\
                          "((" + intervalEvent._intervalEnds[1].getQualifiedClassName() + \
                          ")endEvent).get_" + field + "() -\n\t\t\t" + \
                          "((" + intervalEvent._intervalEnds[1].getQualifiedClassName() + \
                          ")startEvent).get_" + field + "();"

    else:
        for field in intervalEvent._intervalMatch:
	    if intervalEvent.isListField(field):
                listLength = getListLength(intervalEvent, field)
                print '\t\t', intervalEvent.getFieldName(field), '= new', \
                      getJavaListFieldType(intervalEvent, field), \
                      '[(int)', listLength, '];'
                print '\t\tfor(int i = 0; i <', listLength, '; i++)'
                print '\t\t\t' + intervalEvent.getFieldName(field) + '[i] = (('+ \
                      intervalEvent._intervalEnds[1].getQualifiedClassName()+ \
                      ')' + event + ').get_' + field + '(i);'
	    else:
                print "\t\t" + intervalEvent.getFieldName(field) + " = ((" + \
                      intervalEvent._intervalEnds[1].getQualifiedClassName() + \
                      ")endEvent).get_" + field + "();"

        for field in intervalEvent._fieldSeq:
	    event = None
	    qualifiedName = None
	    if endEvent.hasField(field):
	         event = 'endEvent'
                 qualifiedName = endEvent.getQualifiedClassName()
            else: 
                if startEvent.hasField(field):
	            event = 'startEvent'
	            qualifiedName = startEvent.getQualifiedClassName()
                else:
                    if (not field == 'startTime') and (not field == 'endTime'):
                        print '\ngenIntervalConstructor() cannot find field '+field+' in either start '+ \
	                    startEvent.getQualifiedClassName()+' or in end '+				\
	                    endEvent.getQualifiedClassName()+' events'

            if (not event == None):
                 if intervalEvent.isListField(field):
#                    print '/* genIntervalConstructor('+intervalEvent.getClassName()+') for list field '+field+' of '+event+' event*/'
                    listLength = getListLength(intervalEvent, field)
                    print '\t\t', intervalEvent.getFieldName(field), '= new', \
                          getJavaListFieldType(intervalEvent, field), \
                          '[(int)', listLength, '];'
                    print '\t\tfor(int i = 0; i <', listLength, '; i++)'
                    print '\t\t\t' + intervalEvent.getFieldName(field) + '[i] = (('+ \
                          intervalEvent._intervalEnds[1].getQualifiedClassName()+ \
                          ')' + event + ').get_' + field + '(i);'
                 else:                
#                     print '/* genIntervalConstructor('+intervalEvent.getClassName()+') for field '+field+' of '+event+' event*/'
                     print "\t\t" + intervalEvent.getFieldName(field) + " = ((" + \
                          qualifiedName + \
                          ")"+event+").get_" + field + "();"
    print "\t}"

        
# --------------------------------------------------------------------------
# generate a method to create a interval instance
# --------------------------------------------------------------------------
def genCreateIntervalInstance(anEvent):
    print '\t/** create new interval instance */'
    print '\tpublic TraceRecord ', \
          'createIntervalInstance(TraceRecord startEvent, TraceRecord endEvent, long startTime, long endTime) throws TraceException {'
    print '\t\treturn new ',anEvent.getIntervalClassName(), '(startEvent, endEvent, startTime, endTime);'	
    print "\t}"


# --------------------------------------------------------------------------
# generate a class for the hw event list
# --------------------------------------------------------------------------
def genHWEventsClass(hwEventList, outputDir):
    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(outputDir, 'Events', "HWEventList.java"), 'w')
        sys.stdout = fd
    genPreamble(".Events")

    print 'public class HWEventList {'
    print ''
    print '\tstatic public final int UNPROGRAMMED = 0;'
    print ''
    print '\tstatic public final int START_EVENT_INDICES = 1;'
    print ''
    print '\tstatic public String getName(int event) {'
    print '\t\treturn _names[event];'
    print '\t}'
    print ''
    print '\tstatic public String getDescription(int event) {'
    print '\t\treturn _descriptions[event];'
    print '\t}'
    print ''
    print '\tstatic public String getMachine(int event) {'
    print '\t\tif(event == UNPROGRAMMED) return "UNDEFINED";'
    prevCount = 0
    for proc in hwEventList.keys():
        print '\t\telse if(event >', str(prevCount), '&& event <', \
              str(prevCount+len(hwEventList[proc])+1), ') return', \
              "\"" + proc + "\";"
        prevCount = prevCount + len(hwEventList[proc])
    print '\t\treturn "UNDEFINED";'
    print '\t}'
    print ''
    print '\tstatic public final int END_EVENT_INDICES = ',prevCount,';'
    print ''
    print '\tstatic private String _names[] = {'
    count = 0	
    for proc in hwEventList.keys():
        for hwevent, hwdescription in hwEventList[proc]:
            print "\t\t\"" + hwevent + "\",  \t/* ",count," */" #, '/*', hwdescription, '*/'
            count = count+1
    print '\t};'
    print ''
    count = 0	
    print '\tstatic private String _descriptions[] = {'
    for proc in hwEventList.keys():
        for hwevent, hwdescription in hwEventList[proc]:
            print "\t\t\"" + hwdescription + "\",  \t/* ",count," */"
            count = count+1
    print '\t};'
    print '}'
    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()
    
