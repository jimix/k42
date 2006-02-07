# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: xcoff_type.awk,v 1.7 2003/12/03 18:52:55 mostrows Exp $
# ############################################################################

BEGIN {name = ""}

{
    if (name != "") {
        printf "#define %s\t%s\n", name, $2
	name = ""
    }
}

/^[^ \t\f\n\r\v]*__TYPE_.*:$/ {
    match ($1, ".*__TYPE[_]*")
    skip = RLENGTH
    match ($1, "([.][0-9]*)*:$")
    name = substr ($1, skip+1, length($1)-RLENGTH-skip)
}
/^.*.lcomm.*__TYPE[_]*[^,]*,.*$/{

    match($2, ".*__TYPE[_]*")
    skip = RLENGTH
    end = match($2, ",")
    sym = substr($2, skip+1, end-skip-1)
    printf "#define %s\t0\n", sym
}
