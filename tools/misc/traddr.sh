#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# This program replaces addresses it reads on stdin with their symbolic
# name according to the symbol table provided as an argument.  If a token
# does not look like an address, it is just passed through unchanged.
# As soon as my patch to addr2line is accepted to binutils, this program
# can go away.
#
#  $Id: traddr.sh,v 1.2 2005/04/27 21:02:38 apw Exp $
# ############################################################################

function work()
{
    local image=$1;

    while read line; do
       for token in $line; do
           if echo $token | grep -q '^0x'; then
               sym=$(powerpc64-linux-addr2line -C -f -e $image $token | 
		       head -n 1);
               echo -n "$token ($sym) ";
           else
               echo -n "$token ";
           fi
       done
       echo;
    done

    return 0;
}

function help()
{
    echo -e "\`$PROGNAME' translates addresses into symbols\n" \
	    "\n"                                               \
	    "Usage: $PROGNAME -e IMAGE\n"                      \
	    " -?, --help      Show this help statement.\n"     \
	    "     --version   Show version statement.\n"       \
	    " -v, --verbose   Turn on tracing.\n"              \
            " -e, --exe IMAGE Use symbol table from IMAGE\n";
}

function main()
{
    PROGNAME=${PROGNAME:-traddr};
    PROGVERSION=${PROGVERSION:-0.1.0};
    Q=1;

    local image;

    #parse the command line arguments
    while [ $# -gt 0 ]; do
        case $1 in
        -\? | --help)
            help; return 1;
            ;;
        --version)
            echo "$PROGNAME $PROGVERSION"; return 1;
            ;;
        -v | --verbose)
            unset Q; shift;
            ;;
        -vv | --very-verbose)
            set -x; shift;
            ;;
        -e | --exe)
            shift; image=$1; shift;
            ;;
        *)
	    help; return 1;
            ;;
        esac
    done

    [ -z "$image" ] && {
        help;
        return 1;
    }

    work $image || {
	echo "$PROGNAME: FAIL: bailing out";
	return 1;
    }

    return 0;
}

main "$@"
exit $?
