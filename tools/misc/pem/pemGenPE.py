#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGenPE.py,v 1.2 2004/12/23 21:54:15 pfs Exp $
#
# Calin what does this do?
# 
# @author Calin Cascaval
# @date   2004

import string, re, os
import pemEvent, pemGenJava

k42FilterEvents = [ 'TRACE_EXCEPTION_PGFLT', 'TRACE_EXCEPTION_PGFLT_DONE',
                    'TRACE_HWPERFMON_PERIODIC'  ]


def genPEClass(peClassName, peEvents):
    fd = open(os.path.join('com', 'ibm', 'PERCS', 'PPEM', 'traceFormat',
			   'Filters', peClassName + ".java"), 'w')
    fd.write("// Automatically generated ... do not modify!\n")

    fd.write("public class " + peClassName + " {\n")

    allFields = []
    for anEvent in peEvents:
        for field in anEvent._fieldSeq:
            allFields.append(anEvent.getSpecifier() + \
                             anEvent.getFieldName(field))
            fd.write("\tprotected " + pemGenJava.getJavaFieldType(anEvent, field) + " " +\
                     anEvent.getSpecifier() + anEvent.getFieldName(field) + \
                     ";\n")
            

    # generate a constructor that initializes everything to None
    fd.write("\tpublic " + peClassName + "() {\n")
    for field in allFields:
        fd.write("\t\t" + field + " = -1;\n")
    fd.write("\t}\n\n")

    # generate for each event a method that stores the fields
    for anEvent in peEvents:
        fd.write("\tpublic void put" + anEvent.getSpecifier() + " (" +
                 anEvent.getClassName() + " event) {\n")
        for field in anEvent._fieldSeq:
            fd.write("\t\t" + anEvent.getSpecifier() + anEvent.getFieldName(field) +
                     " = event.get" + anEvent.getFieldName(field) + "();\n")
        fd.write("\t}\n\n")

    fd.write("}\n")

    
