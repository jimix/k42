# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

proc miscSupport {} {

    proc pl {l} {
        foreach {i} $l {
            puts $i
        }
    }

     proc diffLists {old new} {
        set nlist {}
        foreach {item} $new {
            set idx [lsearch -exact $old $item]
            if {$idx == -1} {
                lappend nlist $item 
            } else {
                set old [lreplace $old $idx $idx] 
            }
        }
        return [list $old $nlist] 
    }
}

miscSupport