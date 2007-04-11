#!/bin/sh

# #######################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: stubgen.sh,v 1.59 2004/08/20 17:30:49 mostrows Exp $
# #######################################################################

#######################################################################
## set the debugging level here
## export SGDBG=1 before running to force debug output
## all temp files are left around
: ${SGDBG=0}
#######################################################################

PROMPT="STUBGEN:>"
if [ $# -lt 2 ]; then
    echo "$PROMPT args: <Arch> <Header> <CC flags>"
    exit 1
fi

PHASE=""

function errtrap {
   echo "*** $file:0::"
   echo "*** error: $PHASE exited with $SGERR::"
   echo "           $SUGGEST:"
   echo ""
   exit $SGERR
}

#######################################################################

#define the CROSS tools to be used for compilation 
PLATFORM=$1
shift 
#echo "$PROMPT platform =$PLATFORM"

#
# this is where most of the support files are
#
#%HF%
dir=@MKTOOLBIN@/stubgen
CPP5=$dir/cpp5.exe


# define the compiler, awk, and nm commands for this platform
CRCC="@CC@"
CRNM="@NM@"
AWK=@AWK@
EXTRACTIT=$dir/@EXTRACTPROG@


########################################################################
#
# This file processes a given C++ header file, generating a header
# file defining an "external" interface to the objects, and an
# assembly file containing the ppc interface code for the class.  For
# a file called Header.H we generate StubHeader.H and StubHeader.s.
# We also generate another small header file (HeaderInfo.h) containing
# the a define statement for the number of methods in each class in
# the source header file.
#
# To summarize the techniques used in the script:  we process the source
# header file in a number of ways:
#     - we parse the header file directly, looking for key constructs
#     - we generate a dummy C++ file which is compiled with classes of
#       interest
#     - we parse dwarf (debugging) info in this .o file to extract more
#       type information about the classes and their methods
#     - we also parse the data section to extra information about the
#       method numbers of the various virtual functions
# All this data is munged together to produce the final header files and
# assembly file.
#
# special note: if this program is called as "stubgen-g", the temporary
# files are not removed
# 
#
# the name I expect to be called; and my debugging name
#

file=$1

#########################################################################
# determine the filenames that we are dealing with
# there are various ideas out there to name the internal class
# a certain way and derive the other classes accordingly
# current version:  IntenalClass:  <class>
#                   XObj        :  X<class> , Meta<class>
#                   StubClass   :  Stub<class>

filename=`basename $file .H`

shift


# determine where to put the generated files
if [ "$1" = "-o" ]; then
    shift
    dirname=$1
    shift
else
    # currently we put then into the current == obj directory
    dirname="."                  # this is the directory from where invoked
fi


myname=stubgen
debugname=${myname}-g
# prefix for temporary files
#
tmp=$dirname/${myname}__$$
tmpfiles="$tmp.*"

rm -f $tmpfiles
#
# list of all temporary files created
#
#
# ensure temporary files are removed if we exit; we skip this if "debugging"
# is turned on
#
if [ $SGDBG -eq 0 ]; then
  trap 'rm -f $tmpfiles' EXIT
  trap 'rm -f $tmpfiles; exit 1' HUP INT TERM
fi

#
# ensure debugging is off for yacc generated (cpp5) program
#
YYDEBUG=0
export YYDEBUG
#


# make sure we have the proper directories for the include files 
# to be generated

if [ ! -d $dirname/stub ]; then mkdir $dirname/stub; fi
if [ ! -d $dirname/meta ]; then mkdir $dirname/meta; fi
if [ ! -d $dirname/xobj ]; then mkdir $dirname/xobj; fi
if [ ! -d $dirname/tmpl ]; then mkdir $dirname/tmpl; fi


# standard arguments we supply to all awk scripts generating code
myawkargs="-v filename=$filename -v dirname=$dirname \
	    -v sourcefilename=$file"

#
# we strip out all include directives and the special STUB and META
# directives from the source header file in preparation for parsing
#
sed -e 's/^#include.*$//' $file >> $tmp.t1.C
#
# we run the C++ compiler through the file to expand local defines,
# strip out ifdef'ed stuff, and (mostly) to remove comments
# we also explicitly filter out special # directives like '#line' 

PHASE="PreProcessing"
SUGGEST="Check your #ifdef's and macros"
$CRCC $* -E $tmp.t1.C > $tmp.H 
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi
sed -e 's/^#.*$//' $tmp.H > $tmp.t1a.C

# we run our own parser over the file, spitting out information about
# each virtual method in classes defined in the file
#
PHASE="SyntaxAnalysis"
SUGGEST="Check your stub annotations and other syntax errors"
$CPP5 $tmp.t1a.C > $tmp.X 
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

# next we use the information from the parser to generate a file when
# compiled gives information about mangled names through NM command
PHASE="MangleResolution"
SUGGEST="Check c++ error message above"
$AWK -f $dir/genmangle.awk $myawkargs $tmp.X >> $tmp.t2.C
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi
if [ "$PLATFORM" = "mips64" ]; then
    $CRCC $* -S $INCDIR $tmp.t2.C -o $tmp.t2.s
    SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi
    grep '\.dword.*__[0-9]*__'  $tmp.t2.s | \
	$AWK -f $dir/fixupname.awk -v platform=$PLATFORM > $tmp.m5
else
    $CRCC $* -c $INCDIR $tmp.t2.C -o $tmp.t2.o
    SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi
    $CRNM -pu $tmp.t2.o | $AWK -f $dir/fixupname.awk -v platform=$PLATFORM > $tmp.m5
fi
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi
paste -d\; $tmp.m5 $tmp.X > $tmp.m6 

# -----------------------------------------------------------------------
#
# next generate all the header files 
#   stubheader, xheader and metaheader
#   also code will be generated in $tmp.C with
#    constructs referencing the classes found, so that we can examine the
#    executable file to learn more about the classes of interest (in essense,
#    we let the true compiler handle the difficult parsing part)
#

PHASE="ExtractPhase"
#SUGGEST=""
$AWK -f $dir/genhdr.awk $myawkargs $tmp.m6 > $tmp.t3.C
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

$CRCC $* -g -c -MD $INCDIR $tmp.t3.C -o $tmp.t3.o
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

rm -f $filename.stubgen.d
sed -e "1s/^.*\$/Stub$filename.C X$filename.C : \\\\/" $tmp.t3.d > \
							$filename.stubgen.d

# extract the last virtual index valid for virtual function tables
$EXTRACTIT vtable $tmp.t3.o                             | \
	sed -e 's/__VAR_SIZE_VIRTUAL__/_NUM_METHODS_/'	| \
	tr '[a-z]' '[A-Z]'		 	        | \
	awk '{printf("#define %s\t%d\n", $1, $2)}' >> $tmp.nummeths

# extract the virtual indices of functions in a class
if [ "$PLATFORM" = "mips64" ]; then
    $EXTRACTIT vindex $tmp.t3.o | sort -n -k 2,2 | awk '{print $1}' > $tmp.m1
else
    $EXTRACTIT vindex $tmp.t3.o > $tmp.m1
fi

SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

paste -d\; $tmp.m1 $tmp.m6 > $tmp.m4

######################################################################
#
#  Generate the code Stub/Xobj/Meta

PHASE="CodeGenerationPhase"
#SUGGEST=""
$AWK -f $dir/genprog.awk $myawkargs -v methnumsfile=$tmp.nummeths $tmp.m4
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

$AWK -f $dir/gentmp.awk $myawkargs -v methnumsfile=$tmp.nummeths $tmp.m4
SGERR=$?; if [ $SGERR -ne 0 ]; then errtrap; fi

#
# all done
#
# echo "$PROMPT done"
exit 0
#return


