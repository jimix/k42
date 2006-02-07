#!/bin/sh

# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: makepatch.sh,v 1.7 2003/01/24 16:36:41 jimix Exp $
# ############################################################################

if [ \
    \( $# -ne 1 \) -o \
    \( F$1 = "F-?" \) -o \
    \( F$1 = "F-h" \) -o \
    \( F$1 = "F-help" \) -o \
    \( F$1 = "F--?" \) -o \
    \( F$1 = "F--h" \) -o \
    \( F$1 = "F--help" \) \
]; then
    cat << END_OF_HELP
Usage: $0 <patch-script>

Suggested procedure:
    1)  Run "cvs update" on the source machine and fix all conflicts.

    2)  Choose a location (usually, but not necessarily, the root) in
	<srctree>.  Change-directory to the selected <srctree> location
	on the source machine and run "makepatch <patch-script>.

    3)  Ftp <patch-script> to the destination machine in ASCII mode
	and make it executable.

    4)  Do a clean "cvs checkout" on the destination machine.

    5)  Change-directory to the selected <srctree> location on the
	destination machine, and run "<patch-script>".

Note:
    Makepatch explicitly ignores files flagged with '?' by "cvs update".
    Files flagged with 'R' are removed and "cvs delete"d on the destination
    machine, files flagged with 'A' are created and "cvs add"ed, and files
    flagged with 'M' are patched.  Makepatch aborts if "cvs update" reports
    any flags other than these four.
END_OF_HELP
    exit -1
fi

script=$1
update_out=/tmp/makepatch_$$_update_out

trap 'rm -f $update_out' 1 2 3

cvs -f -q -n update > $update_out

if [ $(egrep -c "^[^?RAM] " $update_out) -gt 0 ]; then
    echo "Bailing out - unhandled output from \"cvs update\":"
    echo "================================================================="
    sed -n -e "s/^[^?RAM] /    &/p" $update_out
    echo "================================================================="
    rm -f $update_out
    exit -1
fi

rm -f $script

cat << END_OF_PRE_SCRIPT > $script
#!/bin/sh
#
# Make sure we are run by bash
#
if [ -z "$interp" ]; then
    #
    # Need to make sure we are running bash.
    # We also want to time the script.
    #
    interp=yes
    export interp
    exec sh -c "bash $0 $*"
fi
END_OF_PRE_SCRIPT

if [ $(egrep -c "^\? " $update_out) -gt 0 ]; then
    for f in $(sed -n -e "s/^? //p" $update_out); do
	echo "Unknown:   $f"
    done
fi

if [ $(egrep -c "^R " $update_out) -gt 0 ]; then
    for f in $(sed -n -e "s/^R //p" $update_out); do
	echo "Deleted:   $f"
	echo "echo \"Deleting  $f\""				>> $script
	echo "rm -f $f"						>> $script 
	echo "cvs -f -q delete $f"				>> $script
    done
fi

if [ $(egrep -c "^A " $update_out) -gt 0 ]; then
    for f in $(sed -n -e "s/^A //p" $update_out); do
	echo "Added:     $f"
	echo "echo \"Adding    $f\""				>> $script
	echo "rm -f $f"						>> $script 
	echo "cat > $f << \"\"END_OF_FILE__$f"			>> $script
	cat $f							>> $script
	echo "END_OF_FILE__$f"					>> $script
	echo "(cd $(dirname $f); cvs -f -q add $(basename $f))"	>> $script
    done
fi

if [ $(egrep -c "^M " $update_out) -gt 0 ]; then
    for f in $(sed -n -e "s/^M //p" $update_out); do
	echo "Modified:  $f"
	echo "echo \"Modifying $f\""				>> $script
	echo "patch $f << \"\"END_OF_PATCH__$f"			>> $script
	cvs -f -q diff -c $f					>> $script
	echo "END_OF_PATCH__$f"					>> $script
    done
fi

chmod +x $script

rm -f $update_out
