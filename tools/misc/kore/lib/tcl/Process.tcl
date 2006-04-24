# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

proc ProcessCOSupport {} {
    proc printAddressSpace  {pobj} {
    }

    proc regionListIterator {root func} {
        set val [getField rlst.regions $root Process]
        set node [scalarResp $val]
        set type [classValue $val]
        set rl {}

        while {[string compare $node "0x0"]} {
            lappend rl [$func $node $type]
            set node [scalarResp [getField next $node $type]]
        }
        return $rl
    }

    proc regionHolderValues {ptr type} {
        return [RHS [getInstance $ptr $type] "="]
    }

    proc getRegion {ptr type} {
        return [scalarResp [getField reg $ptr $type]]
    }

    proc getRegionList {p} {
        return [regionListIterator $p regionHolderValues]
    }

    proc getRegions {p} {
        return [regionListIterator $p getRegion]
    }

    proc printRegionList {p} {
        regionListIterator $p printInstance 
    }

    proc getFCMFromReg {reg} {
        return [scalarResp [getCOField fcm $reg]]
    }

    proc regionFCM {ptr type} {
        set reg [getRegion $ptr $type]
        set fcm [getFCMFromReg $reg]
        return [list $reg $fcm]
    }

    proc getRegionFCMs {p} {
        regionListIterator $p regionFCM
    }
    
    proc ps {} {
        processObjs "ProcessDefault|ProcessReplicated" rtnObj
    }

}

ProcessCOSupport