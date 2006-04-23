#!/usr/bin/env bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
#  $Id: knightly.sh,v 1.113 2006/01/26 15:35:28 butrico Exp $
# ############################################################################

#####
# Handy dandy shell functions this script uses
#####
#
# Emit usage of script
#
usage() {
    cat - <<EOF
usage: $prog [options]
    -h		 Show this message and exit.
    -v		 Be verbose
    -D		 Do Not build .dbg files (default for mips64)
    -I		 Do Not use disk image DISK0.0.0
		   (renames image then renames it back when done regress).
    -F           Build fullDeb trees (default is all).
    -P           Build partDeb trees (default is all).
    -N           Build noDeb   trees (default is all).
    -c           Skip cvs checkout
    -K		 Perform clean test before checking in new source.
    -b           Skip building completely (overrides -FPN).
    -r           Skip regress
    -m           Do Not send mail
    -k           Do Not update the knightly script.
    -x           Turn on shell debugging
    -n <#cpus>   Number of CPUs to simulate under simos (default 2)
    -C <cvsroot> Use <cvsroot> as cvsroot.
    -d <dir>     Work in the <dir> directory (default is ./build).
    -R <dir>     Results in the <dir> directory (default is working directory)
    -t <target>	 Build and test for the following target.
		    default depends on host (AIX = powerpc, IRIX = mips64).
    -L <lsrcdir> Directory with Linux sources
    -M <option>  Options to send to make
    -H <victim>	 Run hardware regress, not simulator
    -s <loctn>	 Location of the simulator
    -S <type>    Machine type being simulated
    -f <filesystem> Build this file system
    -z <mail subject> Use this string as subject if sending mail
    -i		 Save part of the install tree

EOF
}

#
# output only if in verbose mode
#
info() {
    test "$verbose" = "yes" && echo "${prog}: " $*
}

#
# So as not to be poisoned by some people's environment...
#
clear_env () {
    # bash makes PATH, PS1, and LOGNAME readonly ...
    # Some of us depend on our environment to specify the timezone

    var_list=$(env | cut -d'=' -f1 | egrep -v '^PATH|PS1|LOGNAME|TZ|GCC3_EXEC|GXX3_EXEC')
    for var in $var_list; do
	unset $var
    done
}

#
# We would like the output of some programs to be indented
#
indent() {
    sed -ne 's/^\(.*\)$/    \1/p'
}

#
# find the full path of a given directory
#
full_path() {
    local owd=$(pwd -P)
    cd $1
    pwd -P
    cd $owd
}

#
# kill all processes of interest belonging to this user
#
ksim() {
    local msg="The following process(es) have to be killed:"
    local kill_list="simos|thinwire|simip|sconsole|k42login"
    local pid_list
    local pid

    pid_list=$(ps -u $(id -nu) -o pid,comm	|
	egrep -e "$kill_list"			|
	awk '{print $1;}')

    if [ -n "$pid_list" ]; then
	for pid in $pid_list; do
	    kill $pid
	    msg="$msg  $pid"
	done
	echo $msg
    fi
}

#
# Check out a new copy of kitchsrc in the current working directory
# Will "rm -rf" the last build if it exists
#
checkout_new_src() {
    local status
    local deb
    # turn on shell debugging if asked to
    test "$shell_debug" = "yes" && set -x

    if [ "$clean_test" = "yes" ]; then
	deb=$(echo $build_target/*Deb)
	info "Performing clean test"
	info "    make $make_opts OBJDIR_OPT_TYPES=\"$deb\" clean"
#	make $make_opts OBJDIR_OPT_TYPES="$deb" clean > $clean_log 2>&1
    fi

    info "Removing current source and products (if they exist)."
    # put everything in the trash so we can get rid of it in the background
    test -d trash || mkdir trash
    test -d kitchsrc && mv -f kitchsrc trash
    test -d install && mv -f install trash
    test -d $build_target && mv -f $build_target trash

    rm -rf trash/* &
    
    SECONDS=0
    info "Checking out a new copy of the source."
    cvs -q checkout kitchsrc > ${co_log} 2> ${co_err}
    status=$?
    checkout_time="$date    $SECONDS"

    return $status
}

#
# Report build errors
#
report_build_error() {
    # turn on shell debugging if asked to
    test "$shell_debug" = "yes" && set -x

    local outfile=$1
    local err_log=$2

    cat - <<EOF
You can find the make log in:
    $outfile

Errors are listed in:
    ${err_log}

The first error can be found at:
    $(head -1 ${err_log})
EOF
}

#
# find build errors
#
check_for_errors() {
    # turn on shell debugging if asked to
    test "$shell_debug" = "yes" && set -x

    local make_status=$1
    local outfile=$2
    local err_log=$3

    if [ $make_status -ne 0 ]; then
	#
	# We search for the 'make [nnn]: *** ' sequence because that will catch
	# link errors as well
	#
	egrep -n '^make\[[0-9]*\]: \*\*\* ' $outfile /dev/null > ${err_log}
	info "Build failed and make exited with $make_status"
	cat - <<EOF >> $mail_log

Build failed and make exited with "$make_status".

$(report_build_error $outfile $err_log)

EOF
    send_mail=err
    else
	#
	# Sometimes we break our makefiles and 'make' thinks it succeeded when
	# it actually did not.  We check for this here, since 'make' thinks it
	# has succeeded
	#
	if egrep -n '^make\[[0-9]*\]: \*\*\* ' $outfile /dev/null > ${err_log}
	  then
	    #
	    # There were errors!!
	    #
	    info "The make command succeeded but the build actually failed."
	    cat - <<EOF >> $mail_log

Makefile Issue!!!
The make command succeeded but the build actually failed.

$(report_build_error $outfile $err_log)

EOF
	    mail_why="build errors"
	    send_mail=err
	else
	    #
	    # No Errors. All is fine. We may run the bits.
	    #
	    info "No build errors occurred."
	    echo "No build errors occurred." >> $mail_log
	    run_regress=yes
	fi
    fi
}    

#
# find build warnings
#
check_for_warnings() {
    # turn on shell debugging if asked to
    test "$shell_debug" = "yes" && set -x

    local outfile=$1
    local warn_log=$2

    fgrep -i -n warning $outfile /dev/null > $warn_log
    if [ ! -s $warn_log ]; then
      # gcc 3.x does not say "warning" so check for any reference to:
      #  '<filepath>:<line>: '
      egrep '^[^:]+:[0-9]+: ' $outfile | uniq > $warn_log
    fi

    # Remove ld's "plt" warnings
    sed -e '/setting incorrect section attributes/d' < $warn_log > tmp.$$ && \
	mv tmp.$$ $warn_log
    rm -f tmp.$$

    if [ -s $warn_log ]; then
	info "Build warnings  occurred."
	cat - <<EOF >> $mail_log
Build warnings have occurred and can be found in:
    $warn_log

Here are the first 5:
$(head -5 $warn_log | indent)

EOF
	mail_why="build warnings"
	send_mail=err
    else
	echo "No build warnings occurred." >> $mail_log
    fi
}

#  clean up before running regress test in case the previous
#  test left mounted fs in a bad state.  
#
cleanup_before_regress() {
    local knfs=$1
    # for NFS fs only
    rm -f $knfs/tmp/regress-sdet.complete
    rm -f $knfs/tmp/regress-sdet.*
    rm -f $knfs/tmp/regress-out
    rm -f $knfs/tmp/status
    rm -f $knfs/tmp/sdet_cpus
    rm -f $knfs/tmp/regress-k42-pass-*
    rm -f $knfs/tmp/regress-linux-pass-*
}

#
# Run the regression tests
#
run_regress_test() {
    # turn on shell debugging if asked to
    test "$shell_debug" = "yes" && set -x

    local deb=$1
    local reg_log=$2
    local kthin_log=$3
    local owd=$(pwd -P)

    if [ "$victim" = "simos" ] ; then
	info "Killing any simulation session in progress"
	local ksim_out=$(ksim)
	if [ -n "$ksim_out" ]; then
	    cat - <<EOF >> $reg_log
WARNING: Prior regress session was running.
$ksim_out
EOF
	fi
    fi
    info "Running regression test for ${deb}."

    SECONDS=0

    # save pwd before changing
    if [ ! -d "$build_dir/$build_target/${deb}/os" ]; then
	cat - <<EOF >> $reg_log
$build_dir/$build_target/${deb}/os: does not exist
Aborting
EOF
	return 1
    fi

    info "cd to os: $build_dir/$build_target/${deb}/os"
    cd $build_dir/$build_target/${deb}/os

    cleanup_before_regress $build_dir/install/$build_target/${deb}/kitchroot

    if [ "$victim" = "mambo" ] ; then
	if [ "$use_DISK_image" = "no" ]; then
	    info "moving DISK0.0.0 to DISK0.0.0.not"
	    mv DISK0.0.0 DISK0.0.0.not
	fi

	export K42_INITSCR=/home/regress/sysinit
	# Force mambo to exit when done
	export MAMBO_EXIT="yes"
	export MAMBO_DIR=$(cd ${simulator_location} && pwd -P)
	export MAMBO_TYPE=${simul_mtype}
	timeout=10800
	info "Running mambo regress"
    else
	info "Running hw regress on ${victim}"
	export K42_INITSCR=/home/regress-hw/sysinit
	timeout=3000
    fi

    export HW_VICTIM=${victim} 
    export OPTIMIZATION=${deb} 
    export K42_NFS_HOST=`hostname`
    test "$verbose" = "yes" && k42console_extra_opts="$k42console_extra_opts -v 3"

    # Make sure the log contains a record of how this run was invoked
    echo k42console -B killsteal -R -D `pwd` -t ${timeout} $k42console_extra_opts >> $reg_log;

    k42console -B killsteal -R -D `pwd` -t ${timeout} $k42console_extra_opts \
		    >>$reg_log 2>&1;

    status=$?

    regress_time="$date  $SECONDS";	

    if [ "$victim" = "mambo" ] ; then
	if [ "$use_DISK_image" = "no" ]; then
	    info "moving DISK0.0.0.not back to DISK0.0.0"
	    mv DISK0.0.0.not DISK0.0.0
	fi
    else
	# Release victim
	hwconsole -x -m ${victim};
	if [ $status -eq 0 ] ; then
	    sdet1=`sed -n -e 's/.*SDET RESULTS 1 \(.*\)$/\1/gp' <$reg_log`
	    sdet2=`sed -n -e 's/.*SDET RESULTS 2 \(.*\)$/\1/gp' <$reg_log`
	    sdet4=`sed -n -e 's/.*SDET RESULTS 4 \(.*\)$/\1/gp' <$reg_log`
	    if [ -n "$sdet1" ]; then sdet1="$date $sdet1"; fi
	    if [ -n "$sdet2" ]; then sdet2="$date $sdet2"; fi
	    if [ -n "$sdet4" ]; then sdet4="$date $sdet4"; fi
	fi
    fi

    if [ $status -eq 0 ] ; then
	info "Status is $status"
	grep -q "Regress Test Complete" $reg_log;
	status=$?
    fi
    if [ $status -eq 0 ] ; then
	info "Detected successful completion"
    else
	info "Status is $status (bad)"
    fi

    info "Regress returned: $status";

    info "cd back to: $owd"
    cd $owd

    linux_status=""
    micro_bench_status=""
    if [ $status -eq 0 ]; then
	info "Regression returned success."
	linux_status=$(fgrep '*** Linux regress failed on pass ' $reg_log)
	if [ -z "$linux_status" ]; then
	    info "Regression test succeeded."
	    cat - <<EOF >> $mail_log
The regression test ran successfully

EOF
        else
	    status=1
        fi
	micro_bench_status=$(fgrep '*** *** Linux micro benchmarks failed on pass ' $reg_log)
	if [ "$victim" != "mambo" ] ; then
	    if [ -z "$micro_bench_status" ]; then
		info "Micro benchmark test succeeded."
		cat - <<EOF >> $mail_log
The micro benchmark test ran successfully

EOF
            else
		status=1
	    fi
        fi
    fi
    if [ -n "$sdet1" ]; then echo $sdet1 >> $sdet1_hist-${deb}; fi
    if [ -n "$sdet2" ]; then echo $sdet2 >> $sdet2_hist-${deb}; fi
    if [ -n "$sdet4" ]; then echo $sdet4 >> $sdet4_hist-${deb}; fi

    if [ $status -ne 0 ]; then
	info "Regression test failed."
	regress_time="$regress_time  regression test failed"

	cat - <<EOF >> $mail_log

The regression test failed! ${linux_status}
You can find the log in:
    $reg_log

Here are the likely causes of failure:

$(egrep --binary-files=text -B 1 -A 2 \
    'Linux regress failed|ltp.sh: Failed| FAIL |ERROR: file' $reg_log |
      tail -n 50)

Here are the last 50 lines:

$(tail -50 ${reg_log} | indent)

EOF
	mail_why=regress
	send_mail=err
    fi

    cat - <<EOF >> $mail_log
Regress Time:
    $regress_time
Last 5:
$(test -s ${reg_hist}-${deb} && tail -5 ${reg_hist}-${deb} | indent)

EOF


    if [ "$victim" != "mambo" ] ; then
	if [ -z "$sdet1" ]; then
	    sdet1="ERROR: sdet benchmark did not run!"
	fi 


	cat - <<EOF >> $mail_log
SDET 1 Time:
    $sdet1
Last 5:
$(test -s ${sdet1_hist}-${deb} && tail -5 ${sdet1_hist}-${deb} | indent)

EOF


	if [ -s ${sdet2_hist}-${deb} ] ; then
	    cat - <<EOF >> $mail_log
SDET 2 Time:
    $sdet2
Last 5:
$(tail -5 ${sdet2_hist}-${deb} | indent)

EOF
	fi

	if [ -s ${sdet4_hist}-${deb} ] ; then
	    cat - <<EOF >> $mail_log
SDET 4 Time:
    $sdet4
Last 5:
$(tail -5 ${sdet4_hist}-${deb} | indent)

EOF
	fi
    fi

    return $status
}

#
# save part of the install tree
#
function copy_install_tree() {
    local deb=$1
    local build_dir=$2
    local result_dir=$3

    local source_dir=${build_dir}/install/powerpc/$deb/
    local target_dir=${result_dir}/install/powerpc/

    mkdir -p ${target_dir}
    cp -pr ${source_dir} ${target_dir}
    rm -rf ${target_dir}/{deb}/kitchroot/boot/boot_image
    rm -rf ${target_dir}/{deb}/kitchroot/klib/*
}

#####
# End of handy dandy functions
#####

# make everything writable (no need to be paranoid)
umask 000

# Make sure that the user has not poisoned the build environment
clear_env

# Used as program name for information and errors
prog=$(basename $0)

# Assume we will always be run from the source directory, for now.
src_dir=$(full_path $(dirname $0))
info "using knightly script in ${src_dir}"

# Who to send mail to on success
mail_success="
k42-status@kitch0.watson.ibm.com
"

# who to send mail to on failure
mail_fail="
k42-status@kitch0.watson.ibm.com
"

# how to send mail (USB style)
mail_cmd=Mail

# default values for stuff
verbose=no		# verbosity
build_dbg=yes		# build .dbg images for programs
checkout=yes		# remove last build and cvs checkout new source
update_knightly=yes	# Make sure this script is up to date
build=yes		# build sources
do_regress=yes		# perform regression test
cpu_num=2		# number os cpus to run regress with
mail_report=yes		# mail a report out
clean_test=no		# perform the clean test
compiler=gcc		# name of the compiler
use_DISK_image=yes	# use the DISK0.0.0 file image
victim="mambo"		# run on mambo  (else specifies victim)
simulator_location=""	# aka K42_MAMBO_DIR
simul_mtype="percs" 	# type of machine being simulated
use_fsystem="NFS"	# use this file system type
mail_sub=""		# part of mail subject
save_install="no"	# save part of the install tree


#extra options for hwconsole
k42console_extra_opts=""

#
# Process Options
#
while getopts "mkxcbhvrFPDINin:t:C:L:M:d:R:p:H:s:S:f:z:" Option; do
    if [ "$verbose" = "yes" ]; then
	echo $Option $OPTARG
    fi

    case $Option in
    h)  usage                  # knightly h -> help
	exit
	;;

# The following options have an argument (note the : in the getopts above)

    n)  cpu_num=$OPTARG		# number of cpus to use in build
	;;
    t)  build_target=$OPTARG	# target architecture for build
	;;
    p)  package_dir=$OPTARG	# directory for the packages
	;;
    L)  linux_src_dir=$OPTARG	# directory for the Linux source code
	;;
    C)  cvsroot=$OPTARG		# CVS root (where to get files from cvs)
	;;
    d)  build_dir=$OPTARG	# directory to work in
	;;
    R)  result_dir=$OPTARG	# directory to put results in
	;;
    M)  make_options="$make_options $OPTARG"   # any user specified 'make' arguments
	;;

# The following options are just flags (yes or no)

    c)  checkout=no		# don't check out files from CVS
	;;
    b)  build=no		# don't bother to build
	;;
    k)  update_knightly=no	# don't check if knightly has been changed
	;;
    K)  clean_test=yes		# Perform clean test before checking in new source.
	echo "Sorry: no clean test support yet" 1>&2
	;;
    r)  do_regress=no		# don't run regression test
	;;
    v)  verbose=yes		# be verbose
	;;
    x)  shell_debug=yes		# set -x for shell and sub-shell invocation
	;;
    m)  mail_report=no		# don't mail the results
	;;
    F)  fullDeb=" fullDeb "	# full debug version
	;;
    P)  partDeb=" partDeb "	# part debug version
	;;
    N)  noDeb=" noDeb "		# no debug version
	;;
    D)  build_dbg_opt=no	# do not build .dbg files
	;;
    I)  use_DISK_image=no	# do not change DISK0.0.0
	;;
    H)
	victim=$OPTARG		# run on hardware
	;;
    s)
	simulator_location=$OPTARG  # ala K42_MAMBO_DIR
	;;
    S)  simul_mtype=$OPTARG	# e.g.,  gpul
	;;
    f)  use_fsystem=$OPTARG	# use this file system type
	;;
    z)  mail_sub="$OPTARG"	# use this subject if sending mail
	;;
    i)	save_install="yes"	# save part of the install tree
	;;
    *)  echo "Illegal option: $Option"
	usage
	exit 1
	;;
    esac
done

# turn on shell debugging if asked to
test "$shell_debug" = "yes" && set -x

# plain path.. to soothe the soul
PATH=/usr/bin:/bin

# Find out what our build host is
# FIXME: when we can actually cross compile this will need more work
ostype=$(uname -s)
info "Running on the ${ostype} OS."

# Set default build target (if it wasn't set explictly by the user)
if [ -z "$build_target" ]; then
    case "$ostype" in
    AIX)    build_target=powerpc
	    ;;

    Linux)  # May not set a default build_target for Linux
	    # use $(uname -m) [machine (hardware) type]
	    # to set a default type in some cases, but this 
	    # assumes we are not doing a cross-compile.
	    lmtype=$(uname -m)
	    case "$lmtype" in
	    ppc*) build_target=ppc64
		  ;;
	    # add other cases here as they become known
	    esac
	    ;;

    *)      echo "Unknown host OS $ostype"
	    usage
	    exit 1
	    ;;
    esac
fi

info "Target architecture is ${build_target}."

# if no build directory specified use a directory from the current one.
if [ -z "$build_dir" ]; then
    build_dir=$(pwd -P)/build
fi

# make sure build directory exists
if [ ! -d $build_dir ]; then
    if ! mkdir -p $build_dir; then
	echo "$build_dir: does not exist!"
	build_dir=$(pwd -P)/build
	echo "using defualt $build_dir"
	mkdir -p "$build_dir"
    fi
fi

# if no result directory specified use the build directory.
if [ -z "$result_dir" ]; then
    result_dir=$build_dir
fi

# make sure result directory exists
if [ ! -d $result_dir ]; then
    if ! mkdir -p $result_dir; then
	echo "$result_dir: does not exist!"
	result_dir=$build_dir
	echo "using default $result_dir"
    fi
fi



#
# make build dir our current working dir
# just in case we need to restart save dir we were run from
#
run_dir=$(pwd -P)
info "Working directory is ${build_dir}."
info "Results directory is ${result_dir}."
cd $build_dir

# This is the first option to make
make_opts="-C kitchsrc"

# check build_target and set environment up for that target
case "$build_target" in
mips64)		
    # FIXME: need something better than this.
    # set up PATH
    PATH=/usr/gnu/bin:${PATH}:/usr/sbin
    export PATH

    # FIXUP compiler location of gcc since UofT built it for us
    COMPILER_PATH=/u/kitchawa/tools/sgi/gcc/lib/gcc-lib/mips-sgi-irix6.4/2.95.2
    export COMPILER_PATH
    LIBRARY_PATH=/u/kitchawa/tools/sgi/gcc/lib/gcc-lib/mips-sgi-irix6.4/2.95.2
    LIBRARY_PATH=${LIBRARY_PATH}:/u/kitchawa/tools/sgi/gcc/lib
    export LIBRARY_PATH

    # we generally do not build .dbg images on mips because they are so HUGE
    build_dbg=no

    # where to find packages if not correct in Make.paths
    package_dir=${package_dir:=~kitchawa/k42-packages.off/mips64}

    ;;

powerpc) 
    # FIXME: need something better than this.
    # set up PATH
    if [ "`uname`" = "Linux" ] ; then
	ARCH=`uname -m`;
	PATH=${PATH}:/home/kitchawa/k42/bin;
	PATH=${PATH}:${build_dir}/install/tools/Linux_${ARCH}/powerpc
    else
	OLDPATH=${PATH}
	PATH=/usr/java14/bin:${PATH}
	PATH=/usr/contrib/bin:/usr/agora/bin:${PATH}
	PATH=/usr/gnu/bin:/usr/local/bin:${PATH}
        PATH=/a/kitch0/homes/kitch0/kitchawa/tools/aix/bin:${PATH}
	PATH=${build_dir}/install/tools/AIX/powerpc:${PATH}
	PATH=/a/kix/homes/kix/kitchawa/k42/bin:${PATH}
	PATH=${PATH}:/u/kitchawa/bin
	PATH=${PATH}:${OLDPATH}
    fi
    export PATH
    compiler=powerpc64-linux-gcc
    ;;

amd64) 
    # /usr/local/bin is needed only for Peterson's gcc
    export PATH=/usr/local/bin:${PATH}
    compiler=${GNUPRE}gcc
    ;;

generic64) 
    # patterned after amd64 for now
    export PATH=/usr/local/bin:${PATH}
    compiler=${GNUPRE}gcc
    ;;
*)
    echo "Unknown build target $build_target"
    echo "Must be either mips64, powerpc, amd64, or generic64"
    usage
    exit 1
    ;;
esac

# fix make options for the file system
# make a kfs build always
case $use_fsystem in
"NFS")
    # The nfs files system is the default.
    ;;
"KFS")
    make_opts="$make_opts FILESYS_DISK_IMAGE=1"
    export K42_SKIP_NFS=true
    export K42_ROOT_FS=KFS
    export K42_PKGVER=3  # We have not made a new disk image yet
    k42console_extra_opts="${k42console_extra_opts} -- K42_FS_DISK=/dev/mambobd/0:/kkfs:kfs:kroot,/dev/mambobd/1:/:kfs:RM"
    # This is slower than with thinwire for regress.  Disabled till
    # we can find out why.
    #k42console_extra_opts="${k42console_extra_opts} -- K42_MAMBO_USE_THINWIRE=0"
    #k42console_extra_opts="${k42console_extra_opts} -- K42_IOSOCKET_TRANSPORT=linux"
    ;;
*)
    echo "Unknown file system type"
    usage
    exit 1
    ;;
esac

# add Linux Source Directory
test -n "$linux_src_dir" && make_opts="$make_opts LINUXSRCROOT=$linux_src_dir"

# add any make options from the user on the knightly command line
make_opts="$make_opts""$make_options"

# The options should override the above settings
test -n "$build_dbg_opt" && build_dbg=$build_dbg_opt
if [ "$build_dbg" != "yes" ]; then
    make_opts="$make_opts NODEBUG_TARGETS=1"
    info "Will not build .dbg files"
fi

# what build types to perform
build_list="${fullDeb}${partDeb}${noDeb}"
# if the above is still blank then build them all.
if [ -z "$build_list" ]; then
    build_list="fullDeb partDeb noDeb"
fi

info "Will act on: ${build_list}."

# Setup cvs
export CVSROOT=${CVSROOT:=${cvsroot:=/home/kitchawa/cvsroot}}

# By default, do not send mail to everyone
send_mail=success

# keep statistics on phases here
sdet1_hist=${result_dir}/sdet1_results
sdet2_hist=${result_dir}/sdet2_results
sdet4_hist=${result_dir}/sdet4_results
reg_hist=${result_dir}/regress_times
co_hist=${build_dir}/checkout_times
co_log=${build_dir}/checkout_log
co_err=${build_dir}/checkout_err
make_hist=${build_dir}/make_times

date=$(date +%Y-%b%d-%H:%M)

# Output known globals that affect us
if [ "$verbose" = "yes" ]; then
  cat - <<EOF
Environment variables:
    date		= $date
    build_list		= $build_list
    (-t) build_target   = $build_target
    (-d) build_dir      = $build_dir
    (-r) result_dir     = $result_dir
    (-D) build_dbg     	= $build_dbg
    (-C) CVSROOT        = $CVSROOT
    (-L) LINUXSRCROOT   = $linux_src_dir
    make_opts		= $make_opts
    PATH		= $PATH

Commands:
    $(type expect)
    $(type make)
        $(make -v | head -1)
    $(type $compiler)
$( ($compiler -v 2>&1) | indent | indent)

EOF
fi

#
# Checkout the source
#
checkout_failed=no
if [ "$checkout" = "yes" ]; then
    if checkout_new_src; then
	echo "$checkout_time" >> ${co_hist}
    else
	info "Checkout of source failed."
	checkout_failed=yes
	# turn off the remainder of the build
	update_knightly=no
	build=no
	do_regress=no
	send_mail=err
	why_mail=checkout
    fi
else
    info "No cvs checkout performed."
fi

#
# Make sure knightly script is up to date
#
if [ "$update_knightly" = "yes" ]; then
    knightly_src=${build_dir}/kitchsrc/tools/misc/knightly.sh
    knightly_dst=${src_dir}/${prog} 

    if ! diff ${knightly_src} ${knightly_dst} 2>&1 > /dev/null; then
	info "This knightly script needs updating"
	cp -f  ${knightly_dst} ${knightly_dst}.last
	cp -f ${knightly_src} ${knightly_dst}
	chmod +x ${knightly_dst}

	# rerun but make sure we do it in the original dir
	cd $run_dir
	info "rerunning knightly with updated script"
	exec bash ${knightly_dst} -ck "$@"
    else
	info "The knightly script is up to date."
    fi
else
    info "Not updating the knightly script"
fi

#
# craft the log dir after we are sure we will not be rerun
#
result_log_dir=${result_dir}/$date
build_log_dir=${build_dir}/$date
mail_log_base=${result_log_dir}/mail_log
mail_msg=${result_log_dir}/mail_msg

# make result_log_dir and build_log_dir. should always happen
if [ ! -d "$result_log_dir" ]; then
    mkdir $result_log_dir
fi
if [ ! -d "$build_log_dir" ]; then
    mkdir $build_log_dir
fi

# create a symlink to the current build as a convenience
test -L ${result_dir}/latest && rm -f ${result_dir}/latest
test -L ${make_dir}/latest && rm -f ${make_dir}/latest
test -e ${result_dir}/latest || ln -s ${result_log_dir} ${result_dir}/latest
test -e ${build_dir}/latest || ln -s ${build_log_dir} ${build_dir}/latest

#
# make sure regressmail is blank
#
for deb in $build_list; do
    run_regress="no"
    warn_log=${build_log_dir}/build_warnings-${deb}
    err_log=${build_log_dir}/build_errs-${deb}
    reg_log=${result_log_dir}/reg_log-${deb}
    kthin_log=${result_log_dir}/kthin_log-${deb}
    make_log=${build_log_dir}/make_log-${deb}

    mail_log=${mail_log_base}-${deb}

    # make sure the following files exist
    cp /dev/null $mail_log		# must be empty

    SECONDS=0
    if [ "$build" = "yes" ]; then
	info "Building ${deb} using:"
	info "    make $make_opts OBJDIR_OPT_TYPES=$deb full_snapshot"
	make $make_opts OBJDIR_OPT_TYPES=$deb full_snapshot > $make_log 2>&1
	make_status=$?
	make_time="$date    $SECONDS"
	info "Finished build phase for ${deb} on ${make_time}."

	#
	# Check if make failed
	#
	check_for_errors  $make_status $make_log $err_log

	#
	# Report any build warnings
	#
	check_for_warnings $make_log $warn_log

        echo $make_time >> ${make_hist}-${deb}
    else
	info "Not building for ${deb}."
	echo "skipping the make process" > $make_log 2>&1
	make_time="$date    Build was skipped"
	# we assume it's already built and will allow regress to run
	run_regress="yes"
    fi

    if [ "$do_regress" = "yes" -a "$run_regress" = "yes" ]; then
	run_regress_test $deb $reg_log $kthin_log
    else
	info "Not performing a regression test for ${deb}."
	regress_time="$date    regress not done"
    fi
    echo "$regress_time" >> ${reg_hist}-${deb}

    if [ "$save_install" = "yes" ]; then
	copy_install_tree $deb $build_dir $result_log_dir
    fi
done

#
# Header for failed mail messages
#
if [ "$send_mail" = "err" ]; then
    cat - <<EOF >> $mail_msg

You are receiving this mail either because there were warnings in the
build, the build failed, or the regress script failed.

EOF

    if [ "$checkout_failed" = "yes" ]; then
	cat - <<EOF >> $mail_msg
Checkout of source failed.

You can find the cvs checkout log in:
    $co_log

Errors are in:
    $co_err

Errors reported:
$(cat $co_err | indent)

EOF
    fi
fi

#
# Construct final mail message
#
cat - <<EOF >> $mail_msg

Below are the results from the automatic
build and regression script run this morning:

This checkout:
    $checkout_time
Last 5 checkouts:
$(test -s ${co_hist} && tail -5 ${co_hist} | indent)
EOF

#
# report on all builds
#
for deb in $build_list; do
    cat - <<EOF >> $mail_msg

======================================================================
results for ${deb}:

$(cat ${mail_log_base}-${deb})

Make Time:
Last 5:
$(test -s ${make_hist}-${deb} && tail -5 ${make_hist}-${deb} | indent)

EOF
done

#
# report shell time statistics
#
tk_times=${result_log_dir}/knightly_times
times > $tk_times

cat - <<EOF >> $mail_msg
processes     user   system
=========    ======= =======
$(sed -ne '1s/^/shell        /p;2s/^/children     /p' ${tk_times})
EOF

rm -f ${tk_times}

#
# Send to display message
#
if [ "$mail_report" == "yes" ]; then
    #
    # finally mail out to the interested parties
    #
    if [ "$build" = "no" ]; then
        cache="reuse"
    else
        cache="clean"
    fi
    if [ "$mail_sub" = "" ]; then
	#mail_sub="${build_target}-${use_fsystem}-${victim}-${cache}"
        #            powerpc         NFS            k4        reuse
        #            powerpc         KFS            stable-percs
        mail_sub="$victim with $use_fsystem"
    fi
    if [ "$send_mail" = "err" ]; then
	mail_subject="FAIL: ${mail_sub}: *** ${mail_why}"
	$mail_cmd -s "$mail_subject" $mail_fail < $mail_msg
    else
	mail_subject="PASS: ${mail_sub}"
	$mail_cmd -s "${mail_subject}" $mail_success < $mail_msg
    fi
else
    #
    # If you don't want it mailed, then we cat it
    #
    cat $mail_msg
fi

#
# On success, of a checkout, build, regress for alldebug levels,
# update RegressStamp
#
if [ "$send_mail" = "success" -a \
     "$checkout" = "yes" -a \
     "$build" = "yes" -a \
     "$do_regress" = "yes" -a \
     "$build_list" = "fullDeb partDeb noDeb" ] ; then
     pushd ${build_dir}/kitchsrc;
     echo "$date" > RegressStamp;
     cvs commit -m "Successful regression test" RegressStamp;
     popd
fi

#
# Return 0 on success and -1 otherwise
#
if [ "$send_mail" != "success"  ]; then
    exit -1
fi
