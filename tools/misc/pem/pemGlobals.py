#! /usr/bin/env python

#
# (C) Copyright IBM Corp. 2004
#
# $Id: pemGlobals.py,v 1.14 2005/05/09 21:02:15 cascaval Exp $
#
# Global variable definitions
#
# @author Calin Cascaval
# @date   2004

language = 'Java'               # the language for generated classes
dialect  = 'K42'                # the dialect for generating C calls
debugLevel = 0                  # debug verbosity
strictXMLChecking = 0           # whether or not we are strict in checking the
                                # XML syntax
hasHWEventList = 0              # are there any HW events defined
genTraceStubs = 0               # whether to generate or not fprintf stubs
oldPythonVersion = 0            # whether we are running on Python 1.x (kelf)
dummyOutput = 0                 # run through code gen without producing files
genCppPe2 = 0                   # generate code for the PE2 filter

traceLayout = 'xml'             # default for the layout of the fields in
                                # the trace file. Choices are 'xml', 'opt'.
                                # 'xml' is using the order in the event def
                                # 'opt' is sorting the fields by size
