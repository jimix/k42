#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemEvent.py,v 1.49 2005/08/01 22:32:48 pfs Exp $
#
# Class that defines an XML event
# 
# @author Calin Cascaval
# @date   2004

import string, re, pemGlobals, pemTypes

PEMLayers = { 'HW'  : '0',
              'HYP' : '1',
              'OS'  : '2',
              'MON' : '3',
              'JVM' : '4',
              'BPA' : '5',
              'APP' : '6',
              'LIB' : '7', 
              'X10' : '8'         # !!! todo/fixme: reorder layers
              }

def getLayerIdMacro(layer):
    return "TRACE_" + string.upper(layer) + "_LAYER_ID"

def getEventMajorIdMacro(classId):
    return "TRACE_" + string.upper(classId) + "_MAJOR_ID"

def getEventMaskMacro(classId):
    return "TRACE_" + string.upper(classId) + "_MASK"

def getEventSpecifierMacro(anEvent):

    fmtPattern1 = '(?P<cap>[A-Z])(?P<rest>[a-z0-9]+)'
    def capsRepl1(matchObject):
        # print "Matching:", matchObject.string 
        return "_" + matchObject.group('cap') + matchObject.group('rest') 

    fmtPattern2 = '(?P<cap>[A-Z][A-Z]+)'
    def capsRepl2(matchObject):
        return "_" + matchObject.group('cap')
            
    strng = re.sub(fmtPattern1, capsRepl1, anEvent.getSpecifier())
    strng = string.upper(re.sub(fmtPattern2, capsRepl2, strng))
    return "TRACE_" + string.upper(anEvent.getClassId()) + strng

# -----------------------------------------------------------------------
# return an event that corresponds to this name
# this method is used in interval generation
# -----------------------------------------------------------------------
def makeDummyEvent(name):
    newEvent = event()
    newEvent._name = name
    (newEvent._layerId, newEvent._classId, newEvent._specifier) = \
                        string.split(name, '::')
    return newEvent


class event:

    def __init__(self):
        self._name = None
	self._description = None
        self._layerId = None
        self._classId = None
        self._specifier = None
        self._fieldSeq = []       # the list of field names as they appear in XML
        self._traceFields = []    # the fields layout for the event record in a PEM trace
        self._PEtraceFields = []  # the fields layout for the event record in a PE trace
        self._traceLayout = pemGlobals.traceLayout # how are fields layed out
        self._computedFields = [] # computed field name list - not in source or in target trace (only in meta data)
        self._inflatedFields = [] # inflated field name list - fields not in source trace but in target trace
        self._fields = {}         # field dictionary - contains field def
        self._fieldsVisSize = {}  # a map of field names to PE sizes
        self._fieldsDescription = {}  
        self._fieldFormat = {}    # a map of field names to print formats
        self._printFormat = None  # the print format for the event
        self._recPrintData = 0
        self._stateChange = ()	  #  0: type, 1: value
        self._sourceCode = () # a tuple of (type, [pc | file, line])
        self._intervalSpecifier = None  # specifier for the interval
        self._intervalType = None  # start or end interval
        self._intervalPair = None  # name of interval pair
        self._intervalMatch  = []  # a list of fields that must match
        self._intervalFields = []  # a list of fields that are included but don't match
        self._intervalEnds = ()    # a tuple that has the start and end events
        self._aixId = None         # aix trace id
        self._aixOffset = {}       # offset in aix trace
        self._properties = {}      # mechanism to pass values between records

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getQualifierSeparator(self):
        if pemGlobals.language == 'C++':
            return '::'
        elif pemGlobals.language == 'Java':
            return '.'
        else:
            return ''
    
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getClassName(self):
        return self.getSpecifier() + "_Event"

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getQualifiedClassName(self):
        return self.getQualifiedSpecifier() + "_Event"

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getDescription(self):
        return self._description

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getLayerId(self):
        return self._layerId

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getQualifiedLayerId(self):
        if pemGlobals.language == 'Java':
            return "PEM" + self.getQualifierSeparator() + "Layer" + \
                   self.getQualifierSeparator() + self.getLayerId()
        elif pemGlobals.language == 'C++':
            return self.getLayerId() + "Layer"
        else:
            return self.getLayerId()

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getClassId(self):
        return self._classId
    
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getQualifiedClassId(self):
        if pemGlobals.language == 'Java':
            return self.getLayerId() + self.getQualifierSeparator() + \
                   self.getClassId()
        else:
            return self.getQualifiedLayerId() + self.getQualifierSeparator() +\
                   self.getClassId()

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getSpecifier(self):
        if pemGlobals.language == 'Java':
            return self.getClassId() + "_" + self._specifier
        return self._specifier
    
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getQualifiedSpecifier(self):
        if pemGlobals.language == 'Java':
            return self.getLayerId() + self.getQualifierSeparator() + \
                   self.getSpecifier()
        elif pemGlobals.language == 'C++':
            return self.getQualifiedClassId() + self.getQualifierSeparator() +\
                   self.getSpecifier()
        else:
            return self.getQualifiedClassId() + self.getQualifierSeparator() +\
                   self.getSpecifier()

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def hasField(self, fieldName):
        return self._fields.has_key(fieldName)
    
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def addField(self, fieldName, fieldType, fieldDescription):
        self._fieldSeq.append(fieldName);
        self._fields[fieldName] = fieldType, None, None
        self._fieldsDescription[fieldName] = fieldDescription

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def setAIXOffset(self, fieldName, aixOffset):
        self._aixOffset[fieldName] = aixOffset
        
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getAIXOffset(self, fieldName):
        if self._aixOffset.has_key(fieldName):
            return self._aixOffset[fieldName]
        return None

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def addComputedField(self, fieldName, fieldType, \
                         fieldExpression, fieldDescription):
        self._computedFields.append(fieldName)
        self._fields[fieldName] = fieldType, fieldExpression
        self._fieldsDescription[fieldName] = fieldDescription

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def addInflatedField(self, fieldName, fieldType, \
                         fieldValue, fieldDescription):
        self._inflatedFields.append(fieldName)
        self._fields[fieldName] = fieldType, fieldValue
        self._fieldsDescription[fieldName] = fieldDescription

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def addInflatedListField(self, fieldName, fieldType, eltType, fieldSize, fieldDescription):
        self._inflatedFields.append(fieldName);
        self._fields[fieldName] = fieldType, eltType, fieldSize
	self._fieldsDescription[fieldName] = fieldDescription

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def addListField(self, fieldName, fieldType, eltType, fieldSize, fieldDescription):
        self._fieldSeq.append(fieldName);
        self._fields[fieldName] = fieldType, eltType, fieldSize
	self._fieldsDescription[fieldName] = fieldDescription

    # -----------------------------------------------------------------------
    # return the trace layout of the event fields
    # if layout is 'xml' then return the list of fields as specified in the
    # xml definition of the event, otherwise, optimize the layout by
    # returning the sorted list of fields -- 
    # that is, [uint64]*, [uint32]*, [uint16]*, [uint8]*, [list]*, [strings]*
    # -----------------------------------------------------------------------
    def getTraceFields(self):
        
        if self._traceLayout == 'xml': return self._fieldSeq # nothing to do

        if len(self._traceFields) == 0: # not layed out yet!
            fieldMap = { 'uint64' : [],
                         'uint32' : [],
                         'uint16' : [],
                         'uint8': [],
                         'list': [],
                         'string': []
                         }
            for field in self._fieldSeq:
                fieldMap[self._fields[field][0]].append(field)
#	    for field in self._inflatedFields:
#                fieldMap[self._fields[field][0]].append(field)
            for ftype in ['uint64','uint32','uint16','uint8','list','string']:
                if len(fieldMap[ftype]) > 0:
                    self._traceFields = self._traceFields + fieldMap[ftype]
        # print '*****************'
        # for field in self._traceFields: print field, ' ',
        # print '\n'
        return self._traceFields
            
    # -----------------------------------------------------------------------
    # return the trace layout of the event fields in an output PE trace 
    # if layout is 'xml' then return the list of fields as specified in the
    # xml definition of the event, otherwise, optimize the layout by
    # returning the sorted list of fields -- 
    # that is, [uint64]*, [uint32]*, [uint16]*, [uint8]*, [list]*, [strings]*
    # Include inflated fields.
    # -----------------------------------------------------------------------
    def getPETraceFields(self):
        
        if self._traceLayout == 'xml': return self._fieldSeq # nothing to do

        if len(self._PEtraceFields) == 0: # not layed out yet!
            fieldMap = { 'uint64' : [],
                         'uint32' : [],
                         'uint16' : [],
                         'uint8': [],
                         'list': [],
                         'string': []
                         }
            for field in self._fieldSeq:
                fieldMap[self._fields[field][0]].append(field)
	    for field in self._inflatedFields:
                fieldMap[self._fields[field][0]].append(field)
            for ftype in ['uint64','uint32','uint16','uint8','list','string']:
                if len(fieldMap[ftype]) > 0:
                    self._PEtraceFields = self._PEtraceFields + fieldMap[ftype]
        # print '*****************'
        # for field in self._traceFields: print field, ' ',
        # print '\n'
        return self._PEtraceFields
            
    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldName(self, fieldName):
        if fieldName == None: return '0'
        return string.join(('_', fieldName), '')

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldNameByPos(self, pos):
        try:
            return self._fieldSeq[pos]
        except IndexError:
            raise 'Invalid field number '+str(pos)+' in event '+str(self._name)

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldNameStripped(self, fieldName):
        if fieldName == None: return '0'
        return fieldName

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def isListField(self, fieldName):
        if not self._fields.has_key(fieldName): return 0
        return self._fields[fieldName][0] == 'list'

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getListCount(self, fieldName):
        if self._fields[fieldName][2]:
            if self.hasField(self._fields[fieldName][2]):
                if pemGlobals.language != 'C':
                    return self.getFieldName(self._fields[fieldName][2])
        return self._fields[fieldName][2]
        
    #------------------------------------------------------------------------
    # added for getUnqiueId, don't want "_" prefix!	
    #------------------------------------------------------------------------
    def getListCountInXML(self, fieldName):
        return self._fields[fieldName][2]
        

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldType(self, fieldName):
        if self.isListField(fieldName):  return self._fields[fieldName][1]
        return self._fields[fieldName][0]

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldValue(self, fieldName):
        if self.isListField(fieldName):  return self._fields[fieldName][2]
        return self._fields[fieldName][1]

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def setFieldFormat(self, fieldName, format):
        self._fieldFormat[fieldName] = format

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldFormat(self, fieldName):
        if self._fieldFormat.has_key(fieldName):
            return self._fieldFormat[fieldName]
        return pemTypes.getDefaultFieldFormat(self.getFieldType(fieldName))

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getEventId(self):
        if pemGlobals.language == 'C++':
            layer = self.getLayerId()
            classid = self.getQualifiedLayerId() + "::" + \
                      getEventMajorIdMacro(self.getClassId())
            specifier = self.getQualifiedLayerId() + "::" + \
                        getEventSpecifierMacro(self)
        else:
            layer = self.getQualifiedLayerId()
            classid = self.getQualifiedClassId()
            specifier = self.getQualifiedSpecifier()
        return "((" + layer + " << 20) | (" + \
               classid + " << 14) | (" + \
               specifier + "))"

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def getFieldDescription(self, fieldName):
        if not self._fieldsDescription.has_key(fieldName) or \
           self._fieldsDescription[fieldName] == None: 
            return fieldName + " has no description"
        
        return self._fieldsDescription[fieldName]

    def getFieldExpression(self, fieldName):
        if self._computedFields.count(fieldName) == 0: return None
        return self._fields[fieldName][1]

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def printAccessorsOfComputedFields(anEvent, field):

	expr = anEvent.getFieldExpression(field)
        if expr == None: return ""

        fmtPattern = '(?P<fieldName>[a-zA-Z]+)'  
        def computedFieldReplace(matchObject):
            if anEvent.hasField(matchObject.group('fieldName')):
                return "get_" + matchObject.group('fieldName') + "()"
	    else: 
                return matchObject.group('fieldName')

        return re.sub(fmtPattern, computedFieldReplace, expr)

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def printComputedFields(anEvent, field):

        expr = anEvent.getFieldExpression(field)
        if expr == None: return ""

        fmtPattern = '%(?P<fieldName>[a-zA-Z]+)'
        def computedFieldName(matchObject):
	    print '// printComputedFields(',anEvent._name,',',field,') matchObject ',matchObject
            return matchObject.group('fieldName')

        return re.sub(fmtPattern, computedFieldName, expr)


    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    # mechanism to pass values between trace records	
    def addProperty(self, name, field):
        self._properties[name] = field

    def getProperty(self, name):
        return self._property[name]

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def isStateChangeEvent(self):
        return len(self._stateChange) > 0
    
    def getStateChangeType(self):
        if self.isStateChangeEvent():   return self._stateChange[0]
        return None

    def getStateChangeNewState(self):
        if self.isStateChangeEvent():   
	    if self.hasField(self._stateChange[1]):
	        # if field, return accessor
  	        return 'get'+self.getFieldName(self._stateChange[1])+'()'
	    else: 
                return self._stateChange[1]
        return None

    # -----------------------------------------------------------------------
    # -----------------------------------------------------------------------
    def isSourceCodeEvent(self):
        return len(self._sourceCode) > 0
    
    def getSourceCodeType(self):
        if self.isSourceCodeEvent():    return self._sourceCode[0]
        return None

    # -----------------------------------------------------------------------
    # returns the PC in a ADDR2LINE type event
    # -----------------------------------------------------------------------
    def getSourceCodePC(self):
        if len(self._sourceCode) > 2:   return None
        return self._sourceCode[1]

    # -----------------------------------------------------------------------
    # return the source information for a SOURCE_FILE type event
    # -----------------------------------------------------------------------
    def getSourceCodeInfo(self):
        if len(self._sourceCode) < 3:   return None
        return self._sourceCode[1] + ":" + self._sourceCode[2]

    # -----------------------------------------------------------------------
    # In an event that specifies an interval endpoint, save the interval event information.	
    # -----------------------------------------------------------------------
    def addInterval(self, name, type, pair, matchList, fieldList):
#        print 'addInterval('+self._name+','+name+')\n'
        self._intervalSpecifier = name
        self._intervalType = string.upper(type)
        if self._intervalType == 'START':
            self._intervalEnds = (self, makeDummyEvent(pair))
        else:
            self._intervalEnds = (makeDummyEvent(pair), self)
        if (not matchList == None) and len(matchList) > 0:
            self._intervalMatch = string.split(matchList, ",")
        for field in self._intervalMatch:
            if not self._fields.has_key(field):
                raise "Invalid interval match field "+ field + \
                      " in event "+ self._name + " not a valid field!"

        if (not fieldList == None) and len(fieldList) > 0:
	    if fieldList == "*": 
		self._intervalFields = self._fieldSeq
 	    else: 
		self._intervalFields = string.split(fieldList, ",")

        for field in self._intervalFields:
            if not self._fields.has_key(field):
                raise "Invalid interval field "+ field + \
                      " in event "+ self._name + " not a valid field!"

    # -----------------------------------------------------------------------
    # return whether this is an interval event
    # -----------------------------------------------------------------------
    def isIntervalEvent(self):
        return self._intervalType != None

    # -----------------------------------------------------------------------
    # return the type of the event
    # -----------------------------------------------------------------------
    def getIntervalType(self):
        return self._intervalType

    # -----------------------------------------------------------------------
    # return an event that corresponds to the pair event
    # -----------------------------------------------------------------------
    def getIntervalPair(self):
        pair = None
        if self.getIntervalType() == 'END': pair = self._intervalEnds[0]
        else: pair = self._intervalEnds[1]
        return pair
    
    # -----------------------------------------------------------------------
    # return the specifier of the interval event derived from this interval's
    # specifier
    # -----------------------------------------------------------------------
    def getIntervalEventSpecifier(self):
        return self._intervalSpecifier
    
    # return the name of the interval event derived from this interval's name
    def getIntervalEventName(self):
        return string.replace(string.replace(self._name, self._classId,
                                             self._classId + "Interval"),
                              self._specifier,self.getIntervalEventSpecifier())

    #------------------------------------------------
    # factored functionality out of makeIntervalEvent
    #-----------------------------------------------
    def transferClassName(self):	
        newEvent = event()
        newEvent._name = self.getIntervalEventName()
        newEvent._description = "defines an interval event that matches " + \
                                self.getIntervalPair()._name+" and "+self._name
        newEvent._layerId = self._layerId
        newEvent._classId = self._classId + "Interval"
        newEvent._specifier = self.getIntervalEventSpecifier()
        return newEvent

    # -------------------------------------------------------------
    # Generate a new interval event that only contains the intervalMatch and
    # intervalFields fields of the start and end events parameters.
    # Constraint: intervalFields and intevalMatch fields are disjoint.	
    # -------------------------------------------------------------
    def makeIntervalEvent(endEvent, startEvent):

        newIntervalEvent = endEvent.transferClassName();

#        newIntervalEvent._traceLayout = endEvent._traceLayout
        newIntervalEvent.addField("startTime", "uint64", "start interval time")
        newIntervalEvent.addField("endTime",   "uint64", "end interval time")
	if endEvent.getIntervalType() == 'PERIODIC':
            for field in endEvent._fieldSeq:
	        if endEvent.isListField(field):
   		    newIntervalEvent.addListField(field, 'list', \
	                              endEvent._fields[field][2], endEvent._fields[field][3], \
                                      endEvent.getFieldDescription(field))
                else: 
                    newIntervalEvent.addField(field, endEvent.getFieldType(field), \
                                      endEvent.getFieldDescription(field))
	else:
#	    print 'makeIntervalEvent('+endEvent._name+','+startEvent._name+')'
            for field in endEvent._intervalMatch:
	        if endEvent.isListField(field):
                    newIntervalEvent.addListField(field, 'list', \
	                              endEvent._fields[field][1], endEvent._fields[field][2], \
                                      endEvent.getFieldDescription(field))
	        else:
                    newIntervalEvent.addField(field, endEvent.getFieldType(field), \
                                      endEvent.getFieldDescription(field))
	    for field in endEvent._intervalFields:
                if (not newIntervalEvent._fields.has_key(field)) and (field != 'startTime') and (field != 'endTime'):
                    if endEvent.isListField(field):
#         	        print '/* makeIntervalEvent('+endEvent._name+','+startEvent._name+') list field "'+field+'"*/\n'
                        newIntervalEvent.addListField(field, 'list', \
                                                      endEvent._fields[field][1], endEvent._fields[field][2], \
                                                      endEvent.getFieldDescription(field))

	            else:
#         	        print '/* makeIntervalEvent('+endEvent._name+','+startEvent._name+')      field "'+field+'"*/\n'
                        newIntervalEvent.addField(field, endEvent.getFieldType(field), \
                                          endEvent.getFieldDescription(field))

            if endEvent._inflatedFields != None and len(endEvent._inflatedFields) > 0:
	        for field in endEvent._inflatedFields:
  	            if not newIntervalEvent._fields.has_key(field):
                        if endEvent.isListField(field):
                            newIntervalEvent.addListField(field, 'list', \
                                                  endEvent._fields[field][1], endEvent._fields[field][2], \
                                                  endEvent.getFieldDescription(field))
	                else:
                            newIntervalEvent.addField(field, endEvent.getFieldType(field), \
                                              endEvent.getFieldDescription(field))

	    if not startEvent == None:
                for field in startEvent._intervalFields:
                    if not newIntervalEvent._fields.has_key(field):
                        if startEvent.isListField(field):
                            newIntervalEvent.addListField(field, 'list', \
                                                  startEvent._fields[field][1], startEvent._fields[field][2], \
                                                  startEvent.getFieldDescription(field))
	                else:
                            newIntervalEvent.addField(field, startEvent.getFieldType(field), \
                                              startEvent.getFieldDescription(field))

	        if startEvent._inflatedFields != None and len(startEvent._inflatedFields) > 0:
	            for field in startEvent._inflatedFields:
  	                if not newIntervalEvent._fields.has_key(field):
                            if startEvent.isListField(field):
                                newIntervalEvent.addListField(field, 'list', \
                                                      startEvent._fields[field][1], startEvent._fields[field][2], \
                                                      startEvent.getFieldDescription(field))
	                    else:
                                newIntervalEvent.addField(field, startEvent.getFieldType(field), \
                                                  startEvent.getFieldDescription(field))

        newIntervalEvent._printFormat = "Interval "+ startEvent._name + \
                              " %0 " + endEvent._name + " %1"
        fieldNum = 2
        for field in endEvent._intervalMatch:
            newIntervalEvent._printFormat = newIntervalEvent._printFormat+" "+ field + "%" + \
                                  str(fieldNum)
        #    + fieldFormat[endEvent.getFieldType(field)]
            fieldNum = fieldNum + 1
        newIntervalEvent._intervalMatch = endEvent._intervalMatch
        newIntervalEvent._intervalEnds  = endEvent._intervalEnds
        newIntervalEvent._intervalType  = endEvent._intervalType

        return newIntervalEvent
    
    # -----------------------------------------------
    # Generate interval class name
    # -----------------------------------------------
    def getIntervalClassName(self):
        return string.replace(string.replace(self.getClassName(),
                                             self._classId, self._classId + "Interval"),
                              self._specifier,
                              self.getIntervalEventSpecifier())


    # ---------------------------------------------------------------------
    # pack the list of fields of an event into 64 bit arguments
    # and return a map {arg_i} -> field_expression, where field_expression
    # is either the field's name or an expression that packs the field
    # into a 64bit quantity
    # ---------------------------------------------------------------------
    def packFields(self):
        args = {}
        filled = 0
        i = 0
        for field in self.getTraceFields():

            fieldType = self.getFieldType(field)
            if self.isListField(field):
                if filled > 0:
                    args[i] = args[i] + ")"
                    i = i+1
                    filled = 0
                # for C we need an extra argument, the size of the list, which
                # is computed in pemGenPEMAPI
                if pemGlobals.language == 'C':
                    #args[i] = string.replace(self.getListCount(field), '_', '', 1) + \
                    #          "* (" + pemTypes.getTypeBitSize(fieldType) + " >> 3)"
                    #args[i+1] = field
                    args[i] = '___listLength'
                    args[i+1] = '(void *)___tmpBuffer'
                    i = i+2
                else:
                    args[i] = field
                    i = i+1
            
            elif fieldType == 'uint64' or fieldType == 'string':
                if filled > 0:
                    args[i] = args[i] + ")"
                    i = i+1
                    filled = 0
                args[i] = field
                i = i+1
            else:
                typeBitSize = int(pemTypes.getTypeBitSize(fieldType))
                if (filled + typeBitSize) <= 64:
                    if not args.has_key(i):
                        args[i] = "("+pemTypes.getCType('uint64')+ ")("
                    if filled > 0:
                        args[i] = args[i] + "|"
                    args[i] = args[i] + \
                              "((("+pemTypes.getCType('uint64')+")"+field+")<<"+\
                              str(64-filled-typeBitSize) + ")"
                else:
                    i = i + 1
                    args[i] = "("+pemTypes.getCType('uint64')+ ")(" + \
                              "((("+pemTypes.getCType('uint64')+")"+field+")<<"+\
                              str(64-filled-typeBitSize) + ")"
                    filled = 0
                filled = filled + typeBitSize
                
                if filled == 64: 
                    args[i] = args[i] + ")"
                    i = i+1
                    filled = 0 

        if filled > 0:
            args[i] = args[i] + ")"

        return args

    # -----------------------------------------------------------------------
    # return the number of string fields in an event
    # -----------------------------------------------------------------------
    def countStrings(anEvent):
        strings = 0
        for field in anEvent._fieldSeq:
            if anEvent.getFieldType(field) == 'string': strings = strings+1
        return strings
