#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: micro_bench.sh,v 1.1 2005/08/19 01:40:40 butrico Exp $
# ############################################################################
#
#   Micro benchmarks tests
#	gettimeofday
#

this_dir=/tests/linux
export PATH=${this_dir}:/bin:/usr/bin:/sbin:/usr/sbin

test_status=0

# tests need to be run in this directory
#cd ${this_dir}

usage() {
    echo
    echo "usage: $0"
    echo "  $0 [-hvqkspPdnN]"
    echo "    -h    show this message and exit"
    echo "    -v    be verbose about each test"
    echo "    -o    show ouput"
    echo "    -k    do not continue after failures"
    echo "    -s    report uses of unsuported system calls"
    echo "    -d    output `date` before running each command"
    echo "    -t    use TRACEPROC to trace all test invocations"
    exit
}

SOUT_FILE=micro_bench.out.$$
SOUT="> $SOUT_FILE"
# Cannot do this since we do not have a shared file cursor yet
SOUT=

#
# good news getopts(1) is a built in for both ash(1) and bash(1)
#
while getopts "hvokspPdnNt" Option; do
    case $Option in
	h)  usage;;
	v)  VOUT=yes;;
	o)  SOUT=;;
	k)  KEEP_GOING=no;;
        s)  export NOSUPP= ;;
	d)  export RUNDATE=yes;;
	t)  export TRACEPROC="";;
	*)  echo "Illegal option: $Option"
	    exit 1;;
    esac
done

recho() {
    test "$VOUT" = "yes" && echo $*
}

rdate() {
    if test "$RUNDATE" = "yes"; then
	date; date; date;
    fi
}


# For running simple tests
run_test() {
    local info=$1
    shift

    recho -e $info
    rdate
    if ! eval $* $SOUT  ; then 
	test "$VOUT" != "yes" || echo -e $info
	echo -e " -- $*: Failed.\n"
	test -s "$SOUT_FILE" && copy "$SOUT_FILE"
	test "$KEEP_GOING" != "no" || exit 1
	test_status=1
    fi
    return 0
}

gettimeofday_test() {
    run_test '\nregress: run system call throughput test\n' gettimeofday
}

if test "$SKIP_NORMAL" != "yes"; then
    gettimeofday_test
fi

echo -e "micro benchmark finished\n"
rdate

exit $test_status
