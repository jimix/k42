#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: regress.sh,v 1.53 2005/08/19 01:40:40 butrico Exp $
# ############################################################################
#
# things that this implicitly tests:
#	argument passing
#	file reads
#	directory reads
#       unlink
#       rm, rmdir
#	dups
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
    echo "    -p    DO NOT test packages"
    echo "    -P    test packages ONLY"
    echo "    -d    output `date` before running each command"
    echo "    -n    run tests for NFS caching (default is not running them)"
    echo "    -N    run ONLY tests for NFS caching"
    echo "    -t    use TRACEPROC to trace all test invocations"
    exit
}

SOUT_FILE=run_test.out.$$
SOUT="> $SOUT_FILE"
# Cannot do this since we do not have a shared file cursor yet
SOUT=

# Test packages by default
TEST_PACKAGES=yes

# Default is not testing NFS caching
TEST_NFS_CACHING=no

#
# good news getopts(1) is a built in for both ash(1) and bash(1)
#
while getopts "hvokspPdnNt" Option; do
    case $Option in
	h)  usage;;
	v)  VOUT=yes;;
	o)  SOUT=;;
	k)  KEEP_GOING=no;;
	p)  TEST_PACKAGES=no;;
	P)  SKIP_NORMAL=yes
	    TEST_PACKAGES=yes;;
        s)  export NOSUPP= ;;
	d)  export RUNDATE=yes;;
	n)  TEST_NFS_CACHING=yes;;
	N)  TEST_NFS_CACHING=yes
	    SKIP_NORMAL=yes
	    TEST_PACKAGES=no;;
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

copy_test() {
    local t
    rdate
    recho -e "\nregress: Testing copy.\n"
    t=$1
    t=${t##*/}.copy
    copy $1 $t

    recho -e "removing file created on copy_test\n"
    rdate
    if [ ! -s $t ]; then
	echo -e "\nregress: copy $1 $t failed.\n"
    stat $1
    stat $t
	rm -f $t
	return 1
    else
	recho -e "\nregress: copy $1 $t succeeded.\n"
	rm -f $t
	return 0
    fi

}

forkexec_test() {
    # the socket command can daemonize it's self
    run_test '\nregress: Starting echo server background.\n' socket -d

    run_test '\nregress: Run normal forktest.\n' forktest 

    run_test '\nregress: Run directory forktest.\n' forktest -d

    run_test '\nregress: execvp(2) the select test\n' /kbin/runprog select

    run_test '\nregress: execvp(2) the dir test\n' /kbin/runprog dir .

    run_test '\nregress: open cwd then execvp(2) to read it\n'\
		/kbin/runprog -if . dir

    run_test '\nregress: Run Unix Domain Socket test\n' send_socket_test
}

pipe_tests() {
#    run_test '\nregress: Run pipepoll test\n'  pipepoll
    run_test '\nregress: Run pipe_select test\n'  pipe_select
}

do_32_bit_tests() {
    run_test '\nregress: Run 32-bit access test\n' access32 $PWD
    run_test '\nregress: Run 32-bit nanosleep test\n' nanosleep32
    run_test '\nregress: Run 32-bit preadwrite test\n' preadwrite32
}

do_ltp_tests() {
    run_test '\nregress: Run LTP test suite\n' ltp.sh
}

filesystem_tests() {
    local verbose;
    test "$VOUT" = "yes" && verbose=-v

    run_test '\nregress: Run file system test: statfs (using stat -f)\n' \
					stat -f .
    run_test '\nregress: Run file system test: openFlags\n'  \
					openFlags $verbose afile adir
    run_test '\nregress: Run file system test: openUnlink\n' \
					openUnlink $verbose afile
    run_test '\nregress: Run file system test: openWriteFork\n' \
					openWriteFork $verbose afile
    run_test '\nregress: Run file system test: openWriteFork\n' \
					openReadForkRead $verbose afile
    echo "afile" > afile					
    run_test '\nregress: Run file system test: truncate afile 80\n' \
					truncate $verbose afile 80
    run_test '\nregress: Run file system test: mmap\n' \
					mmap afile $1
    run_test '\nregress: Run file system test: fileSharing\n' \
                                        fileSharing $verbose
		
    recho -e "removing file created on openFlags/openUnlink/openWriteFork\n"
    rdate
    rm -f afile
    if [ -e afile ]; then
	echo -e "\nregress: removal of file in filesystem_tests() failed\n"
    fi

    recho -e "\nregress: testing rename on `pwd -P` ...\n"
    rm -rf tmp1 tmp2 foodir
    mkdir tmp1 tmp2
    touch tmp1/f1 tmp2/f2
    mkdir tmp1/d1 tmp2/d2
    run_test '\nregress: Run file system test: rename\n' \
					rename $verbose \
					    tmp1/f1 tmp2/f2 tmp1/d1 tmp2/d2
    rm -rf tmp1 tmp2

    run_test '\nregress: Run file system test: preadwrite\n' preadwrite

    run_test '\nregress: Run file system test: utime\n' utime

    run_test '\nregress: Run file system test: faults\n' faults 0 0 26

    run_test '\nregress: Run daemon test: daemon\n' daemon

    recho -e "\nregress: generic file system tests succeeded\n"
}

dev_test() {
    run_test '\nregress: Testing /dev/null\n'	dev -n
    run_test '\nregress: Testing /dev/tty\n'	dev -t
}

pkg_tests() {
    local t
    local f
    for t in /tests/linux/*-test.sh; do
	test -f $t && . $t
	f=${t#/tests/linux/}
	${f%-test.sh}_testall
    done
}
	    
nfs_caching_tests() {
    local verbose;
    test "$VOUT" = "yes" && verbose=-v

    run_test '\nregress: Run nfs caching test: nfsCaching\n' \
				    nfsCaching $verbose
}

# make sure we don't have left overs from previous tests
rm -rf *.copy afile adir tmp1 tmp2

# make sure copy works
rdate
if ! copy_test $this_dir/regress.sh; then
    echo -e "\nregress: copy_test() cannot fail.\nabort\n"
    exit 1
fi

if test "$SKIP_NORMAL" != "yes"; then
#    dev_test
    forkexec_test
    pipe_tests
    /kbin/traceControl --mask ALL
    filesystem_tests $0
    /kbin/traceControl --mask NONE
    do_32_bit_tests
    do_ltp_tests
fi

test "$TEST_PACKAGES" = "yes" && pkg_tests

test "$TEST_NFS_CACHING" = "yes" && nfs_caching_tests

echo -e "regress.sh finished\n"
rdate

exit $test_status
