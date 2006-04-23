#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemTypes.py,v 1.6 2006/02/28 19:21:58 cascaval Exp $
#
# Type handling routines
#
# @author Calin Cascaval
# @date   Jan 16, 2005

import string, pemGlobals

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def isBasicPemType(type):
    typeTable = [ 'uint8', 'uint16', 'uint32', 'uint64', 'string' ];
    return type in typeTable

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getCType(fieldType):
    xml2CTypeTable = { 'string': 'char *',
                       'uint8' : 'uint8',
                       'uint16': 'uint16',
                       'uint32': 'uint32',
                       'uint64': 'uint64' }
    xml2K42TypeTable = { 'string': 'char *',
                         'uint8' : 'uval8',
                         'uint16': 'uval16',
                         'uint32': 'uval32',
                         'uint64': 'uval64' }
    xml2realCTypeTable = { 'string': 'const char *',
                           'uint8' : 'unsigned char',
                           'uint16': 'unsigned short',
                           'uint32': 'unsigned int',
                           'uint64': 'unsigned long long' }
    if pemGlobals.language == 'C': 
        if pemGlobals.dialect == 'K42':
            return xml2K42TypeTable[fieldType]
        else:
            return xml2CTypeTable[fieldType]
    elif pemGlobals.language == 'C++':
        return xml2CTypeTable[fieldType]
    elif pemGlobals.language == 'Fortran':
        return xml2realCTypeTable[fieldType]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getTypeBitSize(fieldType):
    typeTable = { 'string': "str",
                  'uint8' : "8",
                  'uint16': "16",
                  'uint32': "32",
                  'uint64': "64" }        
    return typeTable[fieldType]

# -----------------------------------------------------------------------
# -----------------------------------------------------------------------
def getTypeRawSize(fieldType, fieldName):
    if pemGlobals.language == 'Java':
        typeSizeTable = { 
            'string': string.join(('((',fieldName,'.length()+1)/8+1)*8'), ''),
            'uint8' : '1',
            'uint16': '2',
            'uint32': '4',
            'uint64': '8'
            }        
        return typeSizeTable[fieldType]
    else:
        typeSizeTable = { 
            'string': string.join(('((strlen(', fieldName, ')+1)/8+1)*8'), ''),
            'uint8' : '1',
            'uint16': '2',
            'uint32': '4',
            'uint64': '8'
            }        
        return typeSizeTable[fieldType]
        
def getDefaultFieldFormat(fieldType):
    formatTable = {
        'string': '%s',
        'uint8' : '%x',
        'uint16': '%x',
        'uint32': '%lx',
        'uint64': '%llx',
        }
    return formatTable[fieldType]

def getFieldFormat(fieldType, fieldFormat):
    formatTable = {
        'string': 's',
        'int' : 'd',
        'int32': 'd',
        'hex': 'x',
        }
    modifierTable = {
        'string': '',
        'uint8' : '',
        'uint16': '',
        'uint32': 'l',
        'uint64': 'll',
        }
    if formatTable.has_key(fieldFormat):
        return '%' + modifierTable[fieldType] + formatTable[fieldFormat]
    else:
        return fieldFormat
    
