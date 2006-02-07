#
# regression makefile
# Author: CC
# Date: 01/16/2005
#
.SUFFIXES:
.SUFFIXES: .c .cc .H .h .o .a .java .class

include ../../Make.rules
# SILENT = @

DIFF = diff --ignore-matching-lines="encoding.*=.*\(.*endian\).*"

JAVAC = javac
JFLAGS = $(CLASSPATH)

PEM_TRACE_DIR = ../../trace/filters
PEM_GENHDRS = $(PEM_TOOLS)/pemGenHdrs.py

.c.o:
	$(CC) -c $(CFLAGS) $(C_INCLUDES) $<

.cc.o:
	$(CXX) -c $(CFLAGS) $(CXX_INCLUDES) $<

default: regress_all

regress_all: regress_k42 regress_aix \
	     regress_c regress_cpp regress_java regress_ftn 

# -----------------  ASCII trace dumper ----------------------------------
trace_reader: PemEvents.H PemEvents.cc $(PEM_TRACE_DIR)/cpp/trace_reader.cc
	$(CP) $(PEM_TRACE_DIR)/cpp/trace_reader.cc .
	$(CXX) -I. $(CXXFLAGS) -o trace_reader -DKNOW_EVENTS trace_reader.cc PemEvents.cc
	$(RM) trace_reader.cc

ascii_reader: $(PEM_TRACE_DIR)/cpp/trace_reader.cc
	$(CP) $(PEM_TRACE_DIR)/cpp/trace_reader.cc .
	$(CXX) -I. $(CXXFLAGS) -o ascii_reader trace_reader.cc
	$(RM) trace_reader.cc

# ----------------------- C interface ------------------------------------
PEM_C_LIBRARY = $(INSTALL_LIB)/libpemCTracing.a
#C_INCLUDES = -I $(PEM_TRACE_DIR)/c

#$(C_LIBRARY):
#	$(SILENT)(cd $(PEM_TRACE_DIR)/c; $(MAKE) $(C_LIBRARY))
#	$(SILENT)cp $(PEM_TRACE_DIR)/c/$(C_LIBRARY) .

testCLib: testCLib.c $(PEM_C_LIBRARY)
	$(SILENT)$(PEM_GENHDRS) -l C -s PEM -o . test.xml
	$(SILENT)$(CC) $(CFLAGS) -o testCLib testCLib.c \
	         $(PEM_C_LIBRARY) -lrt


regress_c: testCLib trace_reader
	@echo "Regress C/PEM ---------------------------------------------- "
	$(SILENT)testCLib c.pem
	$(SILENT)./trace_reader c.pem >& c.output
	-$(DIFF) c.output reference.output

# ----------------------- C++ interface ----------------------------------
CXX_INCLUDES = -I. -I $(INSTALL_INC)/traceFormat
PemEvents.H PemEvents.cc: test.xml ../*.py
	$(SILENT)$(PEM_GENHDRS) -l C++ -o . test.xml

testCppLib: PemEvents.cc testCppLib.cc
	$(SILENT)$(CXX) $(CXXFLAGS) -I. -o testCppLib testCppLib.cc PemEvents.cc

regress_cpp: testCppLib trace_reader
	@echo "Regress C++ ------------------------------------------------ "
	$(SILENT)testCppLib cpp.pem
	$(SILENT)./trace_reader cpp.pem >& cpp.output
	-$(DIFF) cpp.output reference.output

# -------------------- Java interface ------------------------------------
JAVA_PKG_PATH = com/ibm/PEM/Events
CLASSPATH = -classpath .:$(INSTALL_LIB)/pemJava.jar:$(INSTALL_LIB)/pemFilters.jar 

test_java: $(INSTALL_LIB)/pemJava.jar
	$(SILENT)mkdir -p $(JAVA_PKG_PATH)
	$(SILENT)$(PEM_GENHDRS) -l Java test.xml
	$(SILENT)$(JAVAC) $(JFLAGS) $(CLASSPATH) testJava.java

regress_java: test_java trace_reader
	@echo "Regress Java ----------------------------------------------- "
	$(SILENT)java $(CLASSPATH) testJava java.pem
	$(SILENT)./trace_reader java.pem >& java.output
	-$(DIFF) java.output reference.output

java_reader:
	java $(CLASSPATH) com.ibm.PEM.Filters.TraceReader $(TRACE)

#java_lib:
#	$(SILENT)(cd $(PEM_TRACE_DIR); $(MAKE) java_lib)
#	$(SILENT)mv $(PEM_TRACE_DIR)/pemJava.jar .

# ----------------------- K42 interface
regress_k42:
	@echo "Regress C/K42 ---------------------------------------------- "
	$(SILENT)$(PEM_GENHDRS) -l C -s K42 -o . test.xml

# ----------------------- AIX PEM interface
regress_aix:
	@echo "Regress C++/AIX -------------------------------------------- "
	$(SILENT)$(PEM_GENHDRS) -l C++ -s AIX -o . test.xml

# ----------------------- Fortran interface
regress_ftn:
	$(SILENT)$(PEM_GENHDRS) -l Fortran -o . test.xml

# --------------------------- clean
clean:: clean_c clean_cpp clean_java clean_ftn clean_aix clean_k42
	-rm -f *~ core.*
	-rm -f trace_reader.o trace_reader

clean_c:
	-rm -f $(C_LIBRARY) testCLib testCLib.o traceMONClasses.h traceTest.h
	-rm -f c.pem c.output

clean_cpp:
	-rm -f PemEvents.H PemEvents.cc MON.H 
	-rm -f testCppLib testCppLib.o cpp.pem cpp.output

clean_java:
	-rm -rf com *.class PEM.java java.pem java.output pemJava.jar

clean_ftn:
	-rm -f traceTest.c traceTest.fh

clean_aix:
	-rm -f MON.H PemEvents.H PemEvents.cc PEMAIXReader.cc

clean_k42:
	-rm -f traceMONClasses.h traceTest.h
