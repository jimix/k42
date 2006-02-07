#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenCPP.py,v 1.54 2005/06/29 22:16:54 cascaval Exp $
#
# Generate C++ classes for XML event descriptions
#
# @author Calin Cascaval
# @date   2004


import string, re, os, sys
import pemEvent, pemGlobals, pemTypes

cppTypeTable = { 'string': 'char *',
                 'uint8' : 'uint8',
                 'uint16': 'uint16',
                 'uint32': 'uint32',
                 'uint64': 'uint64' }        

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genCPPClass(anEvent):

    print 'namespace', anEvent.getLayerId() + "Layer", '{'
    print ''
    print 'namespace', anEvent.getClassId(), '{'
    print ''
    print '/**'
    print '  * class for event', anEvent._name
    print '  * Automatically generated from XML spec. Do not change!'
    print '  */'
    print 'class', anEvent.getClassName(), ': public TraceRecord {'

    print ''
    if len(anEvent._fieldSeq) > 0:
        print '\tprotected:'
    # data members
    for field in anEvent._fieldSeq:
        print '\t', getFieldType(anEvent, field), \
              anEvent.getFieldName(field), ';\t/**<', \
              anEvent.getFieldDescription(field), '*/'
        # print '\t', anEvent._fields

    # genStreamConstructor(anEvent)
    
    genRecordConstructor(anEvent)

    genGeneratorConstructor(anEvent)

    genDestructor(anEvent)
    
    # genPrintMethod(anEvent)
    genToStringMethod(anEvent)

    genGetters(anEvent)

    if anEvent.isStateChangeEvent():
        genStateChangeInterface(anEvent)

    if anEvent.isSourceCodeEvent():
        genSourceCodeInterface(anEvent)

    if pemGlobals.genCppPe2 != 0:
        genPE2Interface(anEvent)

    print '}; // end class', anEvent.getClassName()

    print '} // end namespace', anEvent.getClassId() + "Layer"

    print '} // end namespace', anEvent.getLayerId() + "Layer"

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genStreamConstructor(anEvent):
    print '\n'
    print '\t/** stream constructor for class readers */'
    print '\t', anEvent.getClassName(), '(TraceInputStream istr)', \
          'throws TraceException, IOException {'
    print '\t\tsuper(istr);'

    # check that the record is of this class
    print '\t\tif(getLayerId() !=', anEvent.getLayerId(), \
          '|| getClassId() !=', pemEvent.getEventMajorIdMacro(anEvent.getClassId()), \
          '|| getSpecifier() !=', pemEvent.getEventSpecifierMacro(anEvent),\
          ')'
    print '\t\t\tthrow new TraceException("event " + getLayerId() + ":" +',\
          'getClassId() + ":" + getSpecifier() + " is not of type', anEvent._name,'");'

    if len(anEvent._fieldSeq) > 0:
        print '\t\tfinal TraceInputStream recordStr =', \
              'new TraceInputStream(getPayloadAsStream());'

    # read the fields off the payload 
    for field in anEvent.getTraceFields(): # anEvent._fieldSeq:
        if anEvent.isListField(field):
            listLength = getListLength(anEvent, field)
            print '\t\t', anEvent.getFieldName(field), '= new', \
                  getListEltType(anEvent, field), \
                  '[', listLength, '];'
            print '\t\tfor(unsigned int i = 0; i <', listLength, '; i++)'
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
            
    print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genRecordConstructor(anEvent):
    print '\npublic:'
    print '\t/** copy constructor for class readers */'
    print '\t', anEvent.getClassName(), \
          '(const TraceRecord &r, const char endianEnc)', \
          ': TraceRecord(r) {'

    # check that the record is of this class
    print '\t\tassert(r.getLayerId() ==', anEvent.getLayerId(), \
          '&& r.getClassId() ==', pemEvent.getEventMajorIdMacro(anEvent.getClassId()), \
          '&& r.getSpecifier() ==', pemEvent.getEventSpecifierMacro(anEvent),\
          ');'

    if len(anEvent._fieldSeq) > 0:
        print '\t\tuint32 __pos = 0;'
        
    # read the fields off the payload in chunks of 64 bytes.
    declareTmp = 1
    byteOffset = 0
    packedFields = anEvent.packFields()
    currentField = 0
    for field in anEvent.getTraceFields(): # anEvent._fieldSeq:
        if anEvent.isListField(field):
            listLength = getListLength(anEvent, field)
            # print '\t\tassert(', listLength, '< r.getPayloadLength());'
            print '\t\t', anEvent.getFieldName(field), '= new', \
                  getListEltType(anEvent, field), \
                  '[', listLength, '];'
            print '\t\tfor(unsigned int i = 0; i <', listLength, '; i++) {'
            _fieldName = anEvent.getFieldName(field) + "[i]"
            _fieldType = getListEltType(anEvent, field)
            _fieldTypeSize = pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                                     _fieldName)
            byteOffset = 0 # force reloading next field
            # retrieve the field
            if anEvent.getFieldType(field) == 'string':
                print '\t\t', _fieldName, '=', \
                      'strdup(r.getPayloadString(__pos));'
                print '\t\t__pos +=', _fieldTypeSize, ';'
            else:
                
                print '\t\tif( ((i*', _fieldTypeSize, ')& 0x7)==0) {'
                print '\t\t\tif((__pos >> 3) < r.getPayloadLength()) {'
                print '\t\t\t__tmp8bytes =',\
                      '*(uint64 *)r.getPayloadString(__pos, 8);'
                print '\t\t\t__tmp8bytes =TR2HO_8(__tmp8bytes, endianEnc);'
                print '\t\t\t__pos += 8;'
                print '\t\t} else { __tmp8bytes = 0; } // added fields '
                print '\t\t}'

                if _fieldTypeSize != '8':
                    print '\t\t', _fieldName, '= (', _fieldType, ')', \
                          '((__tmp8bytes >>', \
                          '((8-((i*',_fieldTypeSize,')&0x7)-',_fieldTypeSize, \
                          ')*8)) &', ((1<<int(_fieldTypeSize)*8)-1) ,');'
                else:
                    print '\t\t', _fieldName, '= __tmp8bytes;'

            print '\t\t}'


        else:
            _fieldName = anEvent.getFieldName(field)
            _fieldType = getFieldType(anEvent, field)
            _fieldTypeSize = pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                                     _fieldName)

            # retrieve the field
            if anEvent.getFieldType(field) == 'string':
                print '\t\t', _fieldName, '=', \
                      'strdup(r.getPayloadString(__pos));'
                print '\t\t__pos +=', _fieldTypeSize, ';'
                byteOffset = 0
            else:
                if declareTmp == 1:
                    print '\t\tuint64 __tmp8bytes;'
                    declareTmp = 0
                # if I cant find the current argument in the packed expr
                # it means that I need to move to the next 8 bytes
                if string.find(packedFields[currentField], field) == -1:
                    byteOffset = 0
                    currentField = currentField + 1
                if byteOffset == 0:
                    print '\t\tif((__pos >> 3) < r.getPayloadLength()) {'
                    print '\t\t\t__tmp8bytes =',\
                          '*(uint64 *)r.getPayloadString(__pos, 8);'
                    print '\t\t\t__tmp8bytes =TR2HO_8(__tmp8bytes, endianEnc);'
                    print '\t\t} else { __tmp8bytes = 0; } // added fields '
                    print '\t\t\t__pos += 8;'

                if _fieldTypeSize != '8':
                    print '\t\t', _fieldName, '= (', _fieldType, ')', \
                          '((__tmp8bytes >>', \
                          ((8-byteOffset-int(_fieldTypeSize))*8),\
                          ') &', ((1<<int(_fieldTypeSize)*8)-1) ,');'
                else:
                    print '\t\t', _fieldName, '= __tmp8bytes;'
                byteOffset = (byteOffset + int(_fieldTypeSize)) % 8


    print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genGeneratorConstructor(anEvent):
    print '\n'
    print '\t/** constructor for trace record generators */'
    print '\t', anEvent.getClassName(), '(int timestamp__',

    # generate the prototype
    # used to compute the record size
    recLen = '8'

    # get the trace layout of the arguments
    packedArgs = anEvent.packFields();

    # generate all filed arguments
    listFieldOrd = 0
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field) and anEvent.getListCount(field) == None:
            print ',\n\t\tuint32', "listLengthInBytes" + str(listFieldOrd),
            listFieldOrd = listFieldOrd + 1
        print ',\n\t\t', getFieldType(anEvent, field), \
              anEvent.getFieldNameStripped(field),
    print ')',

    # compute the actual record length
    listFieldOrd = 0
    for field in packedArgs:
        fn = packedArgs[field]
        if anEvent.hasField(fn) and anEvent.isListField(fn):
            if anEvent.getFieldType(fn) == 'string':
                recLen = recLen + "+0"
                continue
            if anEvent.getListCount(fn) != None:
                recLen = recLen + "+(" +\
                         string.replace(anEvent.getListCount(fn),'_','',1)+\
                         "*" + \
                         pemTypes.getTypeRawSize(anEvent.getFieldType(fn),
                                                 fn) + ")"
            else:
                recLen = recLen + "+listLengthInBytes" + str(listFieldOrd)
            listFieldOrd = listFieldOrd + 1    
        elif anEvent.hasField(fn):
            recLen = recLen + "+" + \
                     pemTypes.getTypeRawSize(anEvent.getFieldType(fn),fn)
        else:
            recLen = recLen + "+8"

    # call parent constructor
    print ' :\n\t TraceRecord(',
    print 'timestamp__,', anEvent.getLayerId(), ',', \
          pemEvent.getEventMajorIdMacro(anEvent.getClassId()), ',' , \
          pemEvent.getEventSpecifierMacro(anEvent),', (',recLen,')) {'

    # if the event has no fields, just return
    if len(anEvent._fieldSeq) == 0:
        print '\t}'
        return

    # write all the records
    print '\t\t/* copy the arguments into the fields of the record, in case\n',
    print '\t\t *  the event gets used for anything else than dumping */'
    for field in anEvent._fieldSeq:
        argField = string.replace(anEvent.getFieldName(field), '_', '', 1)
        if anEvent.isListField(field):
            lenVar = "__" + anEvent.getFieldName(field) + "Len"
            print '\t\t', anEvent.getFieldName(field), '= new ',\
                  getListEltType(anEvent, field), \
                  '[', getListLength(anEvent, field), '];'
            if anEvent.getFieldType(field) == 'string':
                print "\t\tunsigned int " + lenVar + " = 0;"
            print '\t\tfor(unsigned int i = 0; i <', \
                  getListLength(anEvent, field), '; i++) {'
            if anEvent.getFieldType(field) == 'string':
                print '\t\t\t',  anEvent.getFieldName(field), \
                      '[i] = strdup(', argField, '[i]);'
                print '\t\t\t', lenVar, '+=', \
                      pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                              anEvent.getFieldName(field)+'[i]'), \
                      ';'
            else:
                print '\t\t\t',  anEvent.getFieldName(field), \
                      '[i] =', argField, '[i];'
            print '\t\t}'
            if anEvent.getFieldType(field) == 'string':
                print '\t\tsetRecordLength(getRecordLength() +', lenVar, '/8);'
        else:
            if anEvent.getFieldType(field) == 'string':
                lenVar = "__" + anEvent.getFieldName(field) + "Len"
                print '\t\tint', lenVar, '=', \
                      pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                              argField), ';'
                print '\t\t', anEvent.getFieldName(field),'= new char[', \
                      lenVar ,'];'
                print '\t\tmemcpy(', anEvent.getFieldName(field), ',', \
                      argField, ', strlen(', argField, ')+1);'
            else:
                print '\t\t',  anEvent.getFieldName(field), '=', \
                      argField, ';'

    print '\n\t\t/* write the fields into the payload */'
    sortedKeys = packedArgs.keys()
    sortedKeys.sort()
    declareTmp = 1
    for pArg in sortedKeys:
        field = packedArgs[pArg]
        if anEvent.hasField(field):
            if anEvent.isListField(field):
                # write a list of strings
                if anEvent.getFieldType(field) == 'string':
                    print '\t\tfor(unsigned i = 0; i <', \
                          getListLength(anEvent, field), '; i++) {'
                    lenVar = "__" + anEvent.getFieldName(field) + "Len"
                    print '\t\t\t', lenVar, '=', \
                      pemTypes.getTypeRawSize(anEvent.getFieldType(field),
                                              anEvent.getFieldName(field)+'[i]'), ';'
                    print '\t\t\twritePayload((char *)', \
                          anEvent.getFieldName(field), '[i],', lenVar, ');'
                    print '\t\t}'
                else: # write a list that's not of strings
                    lenVar = getTypeSize(anEvent, field)
                    print '\t\twritePayload((char *)', \
                          anEvent.getFieldName(field), ',', lenVar, ');'
            else:
                if anEvent.getFieldType(field) == 'string':
                    lenVar = "__" + anEvent.getFieldName(field) + "Len"
                    print '\t\twritePayload((char *)', \
                          anEvent.getFieldName(field), ',', lenVar, ');'
                else:
                    lenVar = getTypeSize(anEvent, field)
                    print '\t\twritePayload((char *)&', \
                          anEvent.getFieldName(field), ',', lenVar, ');'
        else:
            if declareTmp == 1:
                print '\t\tuint64 __tmp;'
                declareTmp = 0
            print '\t\t__tmp =', packedArgs[pArg], ';',
            print 'writePayload((char *)&__tmp, 8);'

    print '\t}'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genDestructor(anEvent):
    print '\npublic:'
    print "\tvirtual ~" + anEvent.getClassName(), '() {'
    for field in anEvent._fieldSeq:
        if anEvent.getFieldType(field) == 'string':
            print '\t\t',
            if anEvent.isListField(field):
                print 'for(unsigned int i = 0; i <', \
                      getListLength(anEvent, field), '; i++)',
                theField = anEvent.getFieldName(field) + "[i]"
            else:
                theField = anEvent.getFieldName(field)
            print 'free(', theField, ');'
        if anEvent.isListField(field):
            print '\t\tdelete []', anEvent.getFieldName(field), ';'
    print '\t}'


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genToStringMethod(anEvent):
    print '\n\t/* -------------- pretty print ------------------------- */'
    print '\tpublic:'
    # generate the printRecordName method
    print '\tvirtual std::ostream &', \
          'printRecordName(std::ostream &ostr = std::cout) const {'
    print '\t\tostr <<', "\"" + anEvent.getSpecifier() + "\"", ';'
    print '\t\treturn ostr;'
    print '\t}'

    if anEvent._printFormat == None: return

    fmtPattern = '%(?P<argNo>\d+)'
    def fieldReplace(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = anEvent.getFieldNameByPos(fieldNo)
        fieldFormat = anEvent.getFieldFormat(fieldName)
        if anEvent.isListField(fieldName):
            result = "\";\n\t\tfor(unsigned int i = 0; i < "
            result = result + getListLength(anEvent, fieldName) + "; i++) \n"
            result = result + "\t\t\t"
            if fieldFormat[len(fieldFormat)-1] == 'x':
                result = result + "ostr << std::hex << get_"+fieldName+"(i) << \" \";"
            else:
                result = result + "ostr << std::dec << get_"+fieldName+"(i) << \" \";"
            result = result + "\n\t\t ostr << \""
            return result
        else:
            fieldTypeSize = getTypeSize(anEvent, fieldName)
            if fieldFormat[len(fieldFormat)-1] == 'x':
                if fieldTypeSize == '1':
                    return "\" << std::hex << (unsigned int)get_" + fieldName + "() << \""
                else:
                    return "\" << std::hex << get_" + fieldName + "() << \""
            else:
                return "\" << get_" + fieldName + "() << \""

    print '\n'
    print '\tvirtual std::ostream &', \
          'printFields(std::ostream &ostr = std::cout) const {'
    print '\t\tostr <<', \
          "\"" + re.sub(fmtPattern, fieldReplace, anEvent._printFormat)+"\"", \
          ';'
    print '\t\treturn ostr;'
    print '\t}'

# -----------------------------------------------------------------------
# generate one getter for each field
# -----------------------------------------------------------------------
def genGetters(anEvent):
    print '\n\t/* ---------------------- accessors ------------------- */'
    print '\tpublic:'
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field):
            print '\t', getListEltType(anEvent, field),
            print string.join(('get', anEvent.getFieldName(field)), ''), '(',
            print 'uint32 i',
            print ') const { return', anEvent.getFieldName(field),
            print '[i]',
        else:
            print '\t', getFieldType(anEvent, field),
            print string.join(('get', anEvent.getFieldName(field)), ''), '(',
            print ') const { return', anEvent.getFieldName(field),
        print '; }'

# Needs more work for dumping fields to metafile!
#    print '\n\t/* ------------- accessors for computed fileds --------- */'
#    for field in anEvent._computedFields:
#        print '\t', getFieldType(anEvent, field),
#        print string.join(('get', anEvent.getFieldName(field)), ''), '()',
#        print 'const {',
#        print 'return', anEvent.printAccessorsOfComputedFields(field), ';',
#        print '}'
##        print 'return', printAccessorsOfComputedFields(anEvent.getFieldExpression(field)), ';',


    print '\n\t/* --------------------- event id getter --------------- */'
    print '\tstatic unsigned int eventId() { return',anEvent.getEventId(),';}'

    # generate the getClassName (PFS added)
    print "\t char* getClassName() { return \"" + \
          anEvent.getClassName()+"\"; }"


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getFieldType(anEvent, fieldName):
    global cppTypeTable

    if not anEvent.hasField(fieldName):
        return None
    if anEvent.isListField(fieldName):
        return cppTypeTable[anEvent.getFieldType(fieldName)] + " *"
    else:
        return cppTypeTable[anEvent.getFieldType(fieldName)]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getListEltType(anEvent, fieldName):
    return cppTypeTable[anEvent.getFieldType(fieldName)]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getReaderType(anEvent, fieldName):
    readerTypeTable = { 'string': 'String',
                        'uint8':  'Byte',
                        'uint16': 'Short',
                        'uint32': 'Int',
                        'uint64': 'Long' }        
    return readerTypeTable[anEvent.getFieldType(fieldName)]


# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getTypeSize(anEvent, fieldName):
    if anEvent.isListField(fieldName):
        return string.join(('(', getListLength(anEvent, fieldName) ,') * ',
                            pemTypes.getTypeRawSize(anEvent.getFieldType(fieldName),
                                           fieldName)), \
                           '')
    else:
        return pemTypes.getTypeRawSize(anEvent.getFieldType(fieldName), fieldName)
    
# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getListLength(anEvent, field):
    if anEvent.getListCount(field) != None:
        return anEvent.getListCount(field)
    else:
        prevFieldsSize = 0
        for aField in anEvent._fieldSeq:
            if field == aField: break
            prevFieldsSize = prevFieldsSize + int(getTypeSize(anEvent, aField))
        prevFieldsSize = prevFieldsSize / 8
        return "(getPayloadLength() - " + str(prevFieldsSize) + ")"


# -----------------------------------------------------------------------
# generate the methods required by the TraceStateChangeEvent interface
# -----------------------------------------------------------------------
def genStateChangeInterface(anEvent):
    print '\n /** ------------- TraceStateChangeEvent interface ----------- */'
    print '\tvirtual uint32 isStateChangeEvent() const { return 1; }'
    print ''
    print '\tvirtual StateChangeE getStateChangeType() const {',
    print "return " + anEvent.getStateChangeType() + ";",
    print '}'
    print ''
    print '\tvirtual uint64 getState() const {',
    print 'return (', anEvent.getStateChangeNewState(), ');',
    print '}'

# -----------------------------------------------------------------------
# generate the methods required by the TraceSourceCode interface
# -----------------------------------------------------------------------
def genSourceCodeInterface(anEvent):
    print '\n /** -------------- TraceSourceCode interface ---------------- */'
    print '\tvirtual uint32 isSourceCodeEvent() const { return 1; }'
    print ''
    print '\tvirtual SourceCodeE getSourceCodeEventType() const {',
    print "return " + anEvent.getSourceCodeType() + ";",
    print '}'
    print ''
    if anEvent.getSourceCodeType() == 'SOURCE_FILE':
        print '\tvirtual char * getSourceCodeLine() const {',
        print "return \"" + anEvent.getSourceCodeInfo() + "\";",
        print '}'
    else:
        print '\tvirtual char * getSourceCodeLine(const char *executablePath) const {'
        print '\t\tchar cmdLine[1024];'
        print '\t\tsprintf(cmdLine, "addr2line -e %s %llx", executablePath,',
        print "get_" + anEvent.getSourceCodePC() + "()", ');'
        print '\t\tint ret = system(cmdLine);'
        print '\t\tif(!ret) return "??:??";'
        print '\t\t// \\todo call a separate routine that forks and collects the output'
        print '\t\treturn "??:??";'
        print '\t}'
        
# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getHeaderDefine(fileName):
    strng = string.replace(fileName, ".", "_")
    return "__" + string.upper(strng) + "__"

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genPreamble(fileName):
    print '/**'
    print ' * Automatically generated'
    print ' * !!!!!!! Please do not change this file by hand !!!!!!!!!'
    print ' * Event changes should be made in the XML event definition, '
    print ' * code changes in genCPP.py'
    print ' * (C) Copyright IBM Corp. 2004'
    print ' */'
    print ''

    if fileName != None and len(fileName) > 0:
        headerDefineName = getHeaderDefine(fileName)

        print '#ifndef', headerDefineName
        print '#define', headerDefineName

    print ''
    
    print '#include <string.h>'
    print '#include <assert.h>'
    print '#include <iostream>'
    print '#include <TraceFormat.H>'

    if pemGlobals.genCppPe2 != 0:
        print '#include "PE2MetaFile"'
        print '#include "PE2TraceOutputStream"'

    print '\n'

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genEventSwitch(allEvents, allClassIds, outputDir):
    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(outputDir, "PemEvents.H"), 'w')
        sys.stdout = fd
    genPreamble("PemEvents.H")

    for layer in allEvents.keys():
        print "#include <" + layer + ".H>"
    print '\n'

    print 'extern PEM::TraceRecord *getEvent(const PEM::TraceRecord &r,'
    print '                                  const char endianEnc);'

    print '\n#endif /*', getHeaderDefine("PemEvents.H"), '*/'

    if pemGlobals.dummyOutput == 0:
        fd.close()
        fd = open(os.path.join(outputDir, "PemEvents.cc"), 'w')
        sys.stdout = fd

    genPreamble("")
    print '#include "PemEvents.H"'
    print '\n'
    print 'using namespace PEM;\n'

    print 'TraceRecord *getEvent(const TraceRecord &r, const char endianEnc) {'
    print '\tswitch(r.getEventId()) {'

    for layer in allEvents.keys():
        for classId in allEvents[layer].keys():
            for anEvent in allEvents[layer][classId]:
                print '\tcase', anEvent.getEventId(), ':'
                print '\t\treturn new', anEvent.getQualifiedClassName(),\
                      '(r, endianEnc);'

    print '\tdefault: return new TraceRecord(r); // unknown'
    print '\t}'
    print '}'

    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()


# -----------------------------------------------------------------------
# generate the AIX trace reader switch
# -----------------------------------------------------------------------
def genAIXTraceReader(allEvents, allClassIds, outputDir):
    fd = None
    if pemGlobals.dummyOutput == 0:
        fd = open(os.path.join(outputDir, "PEMAIXReader.cc"), 'w')
        sys.stdout = fd
    genPreamble(None)

    # system includes
    print '#include <sys/types.h>'
    print '#include <stdlib.h>'
    print '#include <stdio.h>'
    print '#include "libtrace.h"'
    print '#include "PemEvents.H"'
    print ''

    print 'using namespace PEM;'
    print 'using namespace std;'
    print ''


    # defines
    print '#define DEFAULT_PEM_TRACE_FILE "pemTraceFile.trc"'
    print '#define PEM_MAX_CPUS 128'
    print ''
    print 'static char * aixFields;              // -> start of hook data'
    print 'static uint32 aix_cpuid;              // processor number'
    print 'static uint32 aix_tid;                // thread id'
    print ''
    print 'extern int merge_cpu_traces;  // set on in aix2pem if -m option specified'
    print ''

    # prototypes
    print 'extern "C" int pem_write_hdr(trc_loginfo_t *info, uint64 time);'
    print 'extern "C" void pem_write(trc_read_t *r);'
    print 'extern "C" void pem_write_close();'
    print ''

    # global variables
    print 'static TraceOutputStream pemTraceFile[PEM_MAX_CPUS];'
    print '\n'

    print 'static uint32 current_time_upper = 0;'
    print '\n'

    # don't need the following - just need to ensure some event in each 
    # cycle or wrap the the lower 32-bit timestamp. can use AIX 00A event
    print 'void check_for_wrap( uint64 timestamp ) {'
    print '//  TraceRecord *pemRecord = NULL;'
    print '//  int time_upper = timestamp >> 32;'
    print '//  if (time_upper != current_time_upper) {'
    print '//    current_time_upper = time_upper;'
    print '//    for(int i = 0; i < PEM_MAX_CPUS; i++) {'
    print '//      if(pemTraceFile[i].is_open()) {'
    print '//        pemRecord = new OSLayer::Control::Heartbeat_Event (timestamp,timestamp,i,aix_cpuid,aix_tid);'
    print '//        pemRecord->write(pemTraceFile[i]);'
    print '//        delete pemRecord;'
    print '//      }'
    print '//    }'
    print '//  }'
    print '}'
    print '\n'
    
    print 'int pem_write_hdr(trc_loginfo_t *info, uint64 time ) {'
    print ''
    print '  for(int i = 0; i < info->trci_traced_cpus; i++) {'
    print '    char filename[1024];'
    print '    sprintf(filename, "%s.%d", DEFAULT_PEM_TRACE_FILE, i);'
    print '    pemTraceFile[i].open(filename, PEM_DEFAULT_TRACE_ALIGNMENT);'
    print '    if(pemTraceFile[i].fail()) {'
    print '       fprintf(stderr, "Failed to open %s\\n", filename);'
    print '       return 1;'
    print '    }'
    print ''
    print '    TraceHeader h;'
    print '    h.setPhysicalProcessor(i);'
    print '    h.setTicksPerSecond( 7272812500 );  // from hook 00A (on lpar07)'
    print '    h.setInitTimestamp(time);'
    print '    h.write(pemTraceFile[i]);'
    print ''
    print '    // if user specified merged traces, just do the .0 tracefile'
    print '    if ( merge_cpu_traces ) break;'
    print '  }'
    print '  current_time_upper = time >> 32;'
    print '  return 0;'
    print '}'
    print '\n'

    print 'void pem_write(trc_read_t *r) {'
    print '  //static int firstRecord = 1;'
    print '  //if(firstRecord) { pem_write_hdr(r); firstRecord = 0; }'
    print ''
    print '  TraceRecord *pemRecord = NULL;'
    print '  if (r->trcr_flags&TRCRF_64BITTRACE) {'
    print '     // traced on 64bit kernel, header is 8 bytes'
    print '     aixFields = r->trcri_rawbuf + 8;'
    print '     if (r->trcr_flags&TRCRF_64BIT && r->trcr_flags&TRCRF_GENERIC)'
    print '        aixFields += 4;   // skip first half of D1 in 64-bit generic hook'
    print '  } else {'
    print '     // traced on 32bit kernel, header is 4 bytes'
    print '     aixFields = r->trcri_rawbuf + 4;'
    print '     if (r->trcr_flags&TRCRF_64BIT) {'
    print '       cerr << "64-bit trace hooks on 32-bit kernel not supported" << endl;'
    print '       exit(-1);'
    print '     }'
    print '  }'
    print ''
    print '  aix_cpuid = (r->trcr_flags&TRCRF_CPUIDOK ? r->trcri_cpuid : 0);'
    print '  aix_tid = r->trcri_tid;'
    print '  switch(r->trchi_hookid) {'

    for layer in allEvents.keys():
        for classId in allEvents[layer].keys():
            for anEvent in allEvents[layer][classId]:
                if anEvent._aixId == None: continue
                
                print "\tcase 0x" + anEvent._aixId, ':'
                print '\t\tcheck_for_wrap(r->trcri_timestamp);'
                print '\t\tpemRecord = new', anEvent.getQualifiedClassName(),\
                      '(r->trcri_timestamp',
                print ',\n\t\t\taix_cpuid, aix_tid',
                for field in anEvent._fieldSeq:
                    aixOffset = anEvent.getAIXOffset(field)
                    if aixOffset != None:
                        if anEvent.isListField(field):
                            print ',\n\t\t\t',
                            print '(', getListEltType(anEvent, field), '*)(',
                            print 'aixFields +', aixOffset,')',
                        else:
                            print ',\n\t\t\t',
                            print '*(', getFieldType(anEvent, field), '*)(',
                            print 'aixFields +', aixOffset,')',
                    # else:
                    #    print '0',
                print ');'
                print '\t\tbreak;'
    print '\tdefault: pemRecord = NULL; break;'
    print '  }'
    print '  if(pemRecord != NULL) {'
    print '    if ( merge_cpu_traces == 1 )'
    print '      pemRecord->write(pemTraceFile[0]);'
    print '    else'
    print '      pemRecord->write(pemTraceFile[r->trcri_cpuid]);'
    print '    delete pemRecord;'
    print '  }'
    print '}'
    print '\n'
    
    print 'void pem_write_close()'
    print '{'
    print '   for(int i = 0; i < PEM_MAX_CPUS; i++)'
    print '     if(pemTraceFile[i].is_open()) {'
    print '        cerr << "closing processor " << i << endl;'
    print '        pemTraceFile[i].close();'
    print '     }'
    print '}'
    
    if pemGlobals.dummyOutput == 0:
        sys.stdout = sys.__stdout__
        fd.close()

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def genAllClasses(allEvents, allClassIds, outputDir):
    fd = None
    for layer in allEvents.keys():
        fileName = ""
        if pemGlobals.dummyOutput == 0:
            fileName = layer + ".H"
            fd = open(os.path.join(outputDir, fileName), 'w')
            sys.stdout = fd
        genPreamble(fileName)
        print 'namespace PEM {\n'

        print 'namespace', layer + "Layer", '{'
        print ''
        if allClassIds.has_key(layer):
            print '\t/* enum for event classIds in layer', layer, '*/'
            print '\tenum {'
            for classId, classVal in allClassIds[layer]:
                print '\t\t', pemEvent.getEventMajorIdMacro(classId), \
                      '=', classVal, ','
            print '};'
        print '\n'

        # generate the specifiers enum
        for classId in allEvents[layer].keys():
            if pemGlobals.strictXMLChecking == 1:
                if not allClassIds.has_key(layer) or \
                       allClassIds[layer].count(classId) == 0:
                    raise "Event class " + str(classId) + \
                          " has not been defined for layer " + str(layer)
            print '\t/* enum for the event specifiers in class', classId, '*/'
            print '\tenum {'
            for anEvent in allEvents[layer][classId]:
                # print anEvent.getSpecifier(), ',',
                print '\t\t', pemEvent.getEventSpecifierMacro(anEvent), ','
            for anEvent in allEvents[layer][classId]:
                # generate the specifier for the interval event
                if anEvent.isIntervalEvent() and anEvent.getIntervalType()=='END':
                    print '\t\t', pemEvent.getEventSpecifierMacro(anEvent.transferClassName()), ','
            print '};'

        print '} // end namespace', layer + "Layer"

        # generate the event classes
        for classId in allEvents[layer].keys():
            for anEvent in allEvents[layer][classId]:
                print '\n'
                genCPPClass(anEvent)
        print '} // end namespace PEM'

        print '\n#endif /*', getHeaderDefine(fileName), '*/'

        if pemGlobals.dummyOutput == 0:
            sys.stdout = sys.__stdout__
            fd.close()

        genEventSwitch(allEvents, allClassIds, outputDir)

        if pemGlobals.dialect == 'AIX':
            genAIXTraceReader(allEvents, allClassIds, outputDir)

# ----------------------------------------------------------------------------
# generate the PE2 methods
# ----------------------------------------------------------------------------
def genPE2Interface(anEvent):

    print ''
    print '\t// ------------------------ PE2 stuff ------------------------'
    print '\tpublic:'
    genDumpFieldsToTraceFile(anEvent)
    genDumpFieldsToMetaFile( anEvent)
    genGetNumberOfFields(anEvent)
    genGetName(anEvent)
    genGetDescription(anEvent)
    genGetDefaultView(anEvent)

# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genDumpFieldsToTraceFile(anEvent):

    print '\t// dump fields to trace file'
    print '\tclass PE2TraceOutputStream;'
    print '\tvoid dumpFieldsToTraceFile(PETraceOutputStream &traceFile) {'
    # data members
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field):
            print '\t  for(int i = 0; i <', getListLength(anEvent, field),
            print '; i++)'
            print "\t\ttraceFile.write"+getReaderType(anEvent, field),
            print "(("+ getFieldType(anEvent, field) + ")" + \
                  anEvent.getFieldName(field) + "[i]);"
        else:
            print "\t  traceFile.write"+getReaderType(anEvent, field)+\
                  "(("+ getFieldType(anEvent, field) + ")" + \
                  anEvent.getFieldName(field) + ");"
    #print '\t  System.out.println("***Missing type for field ', \
    #      anEvent.getFieldName(field), '!***");'

    # computed data members -- Peter doesn't want these in the PE trace ...
    #for field in anEvent._computedFields:
    #    print "\t  traceFile.write" + getJavaReaderType(anEvent, field)+ "("+ \
    #          anEvent.getFieldName(field) + ");"

    print '\t}'

# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genDumpFieldsToMetaFile(anEvent):

    print '\t// dump field descriptions to meta file'
    print '\tclass PE2MetaFile;'
    print '\tvoid dumpFieldsToMetaFile(PE2MetaFile &metaFile) {'
    # print '\t  System.out.println("dumpFieldsToMetaFile(): "+toString());'
    # data members
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field):
            print '\t for(int i = 0; i <', getListLength(anEvent, field),
            print '; i++)'
            print "\t\tmetaFile.writePFD_" + \
                  getReaderType(anEvent, field),
            print "(\""+ anEvent.getFieldNameStripped(field) + "\"+i, \"" + \
                  anEvent.getFieldDescription(field) + "\", true);" 
        else:
            print "\t metaFile.writePFD_"+getReaderType(anEvent, field)+\
                  "( \"" + anEvent.getFieldNameStripped(field) + "\", \"" + \
                  anEvent.getFieldDescription(field) + "\", true);"
    # computed data members - Peter doesn't want these in the PE trace
    for field in anEvent._computedFields:
	# Calin can a list be here?  PFS
        print "\t  metaFile.writePFD_" + getReaderType(anEvent, field) + \
              "( \"" + anEvent.getFieldNameStripped(field) + "\", \"" + \
              anEvent.getFieldDescription(field) + "\", true);"
              
    #print '\t  System.out.println("***Missing type for field ', \
    #      anEvent.getFieldName(field), '!***");'

    print '\t}'

# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genGetNumberOfFields(anEvent):
    print '\t// meta file\'s number of fields for an event'
    print '\tint getNumberOfFields() { return',
    allFields = "0"
    for field in anEvent._fieldSeq:
        if anEvent.isListField(field):
            allFields = allFields + "+(int)"+ getListLength(anEvent, field)
        else:
            allFields = allFields + "+1";
    print allFields, ";}"



# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genGetName(anEvent):
    print '\t// meta file\'s name for an event'
    print '\tchar *getName() {'
    print "\t\treturn \"" + string.replace(anEvent._name, "::", ".") + "\";"
    print '\t}'


# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genGetDescription(anEvent):
    print '\t// meta file\'s description for an event'
    print '\tchar *getDescription() {'
    if (anEvent.getDescription() == None):
      print '\t\treturn "no description specified!";'
    else:
      print string.join(('\t\treturn "', anEvent.getDescription(), '";'), '')
    print '\t}'

# -----------------------------------------------------------------------
# PE2 methods
# -----------------------------------------------------------------------
def genGetDefaultView(anEvent):
    if anEvent._printFormat == None: return

    fmtPattern = '%(?P<argNo>\d+)\[%(?P<modif>\w*)(?P<fmt>\w)\]'
    def strippedFieldReplace(matchObject):
        fieldNo = int(matchObject.group('argNo'))
        fieldName = anEvent._fieldSeq[fieldNo]
        if anEvent.isListField(fieldName):
            result = "\";\n\t\tfor(int i = 0; i < "
            result = result + getListLength(anEvent, fieldName) + "; i++) \n"
            result = result + "\t\t\t"
            if matchObject.group('fmt') == 'x':
                result = result + "result += Long.toHexString(get_" + \
                         fieldName + "(i)) + \" \";"
            else:
                result = result + " result += get_"+ fieldName + "(i) + \" \";"
            result = result + "\n\t\tresult += \""
            return result
        else:
            if matchObject.group('fmt') == 'x':
#                return "\\\"+ Long.toHexString(" + fieldName + ") +\\\""
                return "\\\"+ hex(" + fieldName + ")+\\\""
            else:
                return "\\\"+" + fieldName + "+\\\""

    print ''
    print '\tchar * getDefaultView() {'
    print '\t\treturn',
#    print '\"\\\"\"+getSpecifierAsString()+\"\\\"+ timeStamp+',
    print '\"timeStamp+ ',
    print '\\\"'+re.sub(fmtPattern, strippedFieldReplace, anEvent._printFormat)+'\\\""',
    print ';'
    print '\t}'
    print '\n'

#    print '\"timeStamp+\"+getSpecifierAsString()+\" ', timeStamp correct
#    print '\"timeStamp+\\\"\"+getSpecifierAsString()+\"\\\"+',
#    print '\\\"+getSpecifierAsString()+\\\"+timeStamp+',


