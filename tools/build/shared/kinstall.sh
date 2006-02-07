#!/bin/sh

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kinstall.sh,v 1.16 2001/07/11 19:24:47 peterson Exp $
# ############################################################################

# MAA - cygwin b20 is broken - it must have a full path name for interpreter
# and will not default at all
# This is a shell script used to install include files, 
# - the first argument is the target directory to which the files should be 
#   installed, the directory name must end with a "/"
# - the second argument is the source directory from which the files are
#   installed, the directory name must end with a "/"
# - one or more file names follow (with no path)
# this script checks if the target directory exists, and if not creates it
# then for each file, compares the source file to any file with the same
# name in the target directory, and installs the source file if they differ

# see if running under bash
if [ "$BASH_VERSION" = "" ]
then
    export BASH_ENV=${BASH_ENV=}
    bash $0 $*
    exit $?
fi


usage( ) {
       echo -n "usage: $progname targetdir/ sourcedir/ <list of files>"
}


# get what install program to use
INST="@INSTALL@"

targ_dir=$1
	if [ X"$targ_dir" = X ]; then
	    usage
	    exit -1
	fi
	shift

src_dir=$1
	if [ X"$src_dir" = X ]; then
	    usage
	    exit -1
	fi
	shift

# make sure there is exactly one trailing slash
# turn on extended pattern matching
shopt -qs extglob
targ_dir=${targ_dir%%+(/)}/
src_dir=${src_dir%%+(/)}/


# check if target directory exists, else create it
if [ ! -d $targ_dir ] ; then	
     echo "creating directory $targ_dir" 
     $INST --mode 0775 -d $targ_dir
fi

export sflg="start"

while :
do
# Break out if there are no more args
   case $# in
   0)
      break
      ;;
   esac

   ins_file=$1
   shift

   doinst="0"

   if [ ! -f ${targ_dir}$ins_file ] ; then
      doinst="1"
   else 
      [ ! ${src_dir}$ins_file -nt ${targ_dir}$ins_file ] ||
      cmp -s ${src_dir}$ins_file ${targ_dir}$ins_file > /dev/null 2>&1 ||
         doinst="1"
   fi

   if [ $doinst = "1" ]
     then
     {
        if [ $sflg = "start" ] 
          then   
            export sflg="done"
            echo "kinstall: ${src_dir} -> ${targ_dir} :" 
          fi

          echo "   - $ins_file ";
          $INST   --mode 0444 ${src_dir}$ins_file ${targ_dir}; 
      };
      fi
done
