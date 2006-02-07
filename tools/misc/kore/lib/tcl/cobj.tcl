# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#

# expects GDBSupport

proc COSupport {} {

	proc buildTable {c r t} {
		global COTable COIDs
		lappend COIDs $c
		lappend COTable($c) $r
	}

	proc showTable {} {
		global COTable COIDs
		foreach item $COIDs {
			if {[llength COTable($item)]!=1} {
				puts [llength COTable($item)]
			} else {
				puts -nonewline .
			}
			flush stdout
		}
		puts ""
	}
	
	proc setupCOSupport {} {
	}

	proc makeOP {op baseType} {
		eval "proc ${op} {ptr} {
			if {[string compare \$ptr 0x0]==0} {
				return
			}
			foreach type \[isa \$ptr $baseType\] {
				if { \[ info procs \${type}_${op} \]!={} } {
					return \[\${type}_${op} \$ptr\]
				}
			}
			puts \"no $op support for \$ptr isa \[isa \$ptr $baseType\]\"
			return {}
		}"
	}

	# Returns the list of reps associated with root
    makeOP getReps COSMissHandlerBase

    proc CObjRootSingleRep_getReps {root} {
		return [list [scalarResp [getField therep $root CObjRootSingleRep] hexValue]]
    }

	proc CObjRootMultiRepPinned_getReps {root} {
		set ptr [numValue [RHS [getField replist $root CObjRootMultiRepPinned] "head = "]]
		while {[string compare $ptr "0x0"]} {
			set response [getInstance $ptr ListSimpleKeyBase<AllocPinnedGlobal>::ListSimpleKeyNode]
			lappend reps [numValue [RHS $response "datum = "]]
			set ptr [numValue [RHS $response "next = "]]
		}
		return $reps
	}

	proc CObjRootMultiRep_getReps {root} {
		set ptr [numValue [RHS [getField replist $root CObjRootMultiRep] "head = "]]
		while {[string compare $ptr "0x0"]} {
			set response [getInstance $ptr ListSimpleKeyBase<AllocGlobal>::ListSimpleKeyNode]
			lappend reps [numValue [RHS $response "datum = "]]
			set ptr [numValue [RHS $response "next = "]]
		}
		return $reps
	}

	proc CObjRootMultiRepBase_getReps {root} {
		set max [scalarResp [getField maxNumaNodeNum $root CObjRootMultiRepBase]]
		for {set i 0} {$i<=$max} {incr i} {
			set response [getField repByNumaNode\[$i\] $root CObjRootMultiRepBase]
			lappend reps [scalarResp $response]
		}
		return $reps
	}
	
	proc ListSimpleIterator {head func} {
		while {[string compare $head "0x0"]} {
			set response [getInstance $head ListSimpleKeyBase<AllocGlobal>::ListSimpleKeyNode]
			$func [numValue [RHS $response "datum = "]] [numValue [RHS $response "key = "]]
			set head [numValue [RHS $response "next = "]]
		}
	}

	proc HashSimpleIterator {hashTable numNodes func {alloc AllocGlobal}} {
		sendToGDB "set print object off"
		set count 0
		for {set i 0} {1} {incr i} {
			set response [sendToGDB "p (('HashSimpleBase<$alloc, 0ul>::HashNode' *)$hashTable)\[$i\]"]
			set chainHead [scalarResp $response]
			while {[string compare $chainHead "0x0"]} {
				set response [sendToGDB "p *(('HashSimpleBase<$alloc, 0ul>::HashSimpleNode' *)$chainHead)"]
				#puts "[numValue [RHS $response "key = "]] [numValue [RHS $response "datum = "]]"
				$func [numValue [RHS $response "key = "]] [numValue [RHS $response "datum = "]]
				incr count
				if {[expr $count >= $numNodes]} {
					sendToGDB "set print object on"
					return
				}
				set chainHead [numValue [RHS $response "next = "]]
			}
		}
	}

	proc HashSearch {hashTable numNodes key keyShift hashMask func {alloc AllocGlobal}} {
		# If keyShift is of the form 4ul, for example, it extracts the number alone
		if {regexp {([0-9]*)ul} $keyShift dummy realKey} {
			set key realKey
		}
		set index [expr [expr $key >> $keyShift] & $hashMask]
		set response [sendToGDB "p (('HashSimpleBase<$alloc, 0ul>::HashNode' *)$hashTable)\[$index\]"]
		set chainHead [scalarResp $response]
		while {[string compare $chainHead "0x0"]} {
			set response [sendToGDB "p *(('HashSimpleBase<$alloc, 0ul>::HashSimpleNode' *)$chainHead)"]
			if {[numValue [RHS $response "key = "]] == $key} {
				puts "$key [numValue [RHS $response "datum = "]]"
				#$func [numValue [RHS $response "key = "]] [numValue [RHS $response "datum = "]]
				sendToGDB "set print object on"
				return
			} else {
				set chainHead [numValue [RHS $response "next = "]]
			}
		}
		sendToGDB "set print object on"
		puts "key not found"
		return
	}

	# Returns the class associated with rep ptr
	proc getCOType {ptr} {
		return [lindex [isa $ptr COSTransObject] 0]
	}

	proc getRepsFromCOID {coid} {
		if {[string compare $coid "0x0"]==0} {
			return
		}
		return [getReps [getRoot $coid]]
	}
 
	proc setGetRootVars {} {
		global lTransTablePinned lTransTablePaged \
				gTransTablePinned gTransTablePaged

		proc dummy {c r t} {
			return "coid=$c root=$r type=$t"
		}

		set response [processObjs COSMgr dummy]
		set response [getInstance [numValue [RHS [getInstance \
					[numValue [RHS $response "root="]] \
					COSMgrObjectKernObject::COSMgrObjectKernObjectRoot ] "repList = " ]] COSMgrObject]
		set lTransTablePinned [numValue [RHS $response "lTransTablePinned = "]]
		set lTransTablePaged [numValue [RHS $response "lTransTablePaged = "]]
		set gTransTablePinned [numValue [RHS $response "gTransTablePinned = "]]
		set gTransTablePaged [numValue [RHS $response "gTransTablePaged = "]]
		puts "$lTransTablePinned $lTransTablePaged $gTransTablePinned $gTransTablePaged"
		return {}
	}

	proc getRoot {coid} {
		global lTransTablePinned lTransTablePaged \
				gTransTablePinned gTransTablePaged
				
		set gPinnedSize				8192
		set gTransTablePageableSize	0x10000000 
		
		if {[info exists lTransTablePinned]==0 || [expr $lTransTablePinned == 0]} {
			setGetRootVars
		}

		if {[expr [expr [expr $coid >= $lTransTablePinned] && \
			[expr $coid < [expr $lTransTablePinned + $gPinnedSize]]] || \
			 [expr $lTransTablePaged && [expr [expr $coid >= $lTransTablePaged] && \
			 [expr $coid < [expr $lTransTablePaged + $gTransTablePageableSize]]]]] != 1} {
			puts "Invalid coid $coid"
			return
		}
		if {[expr [expr $coid - $lTransTablePinned] < $gPinnedSize] && 
		[expr [expr $coid - $lTransTablePinned] >= 0]} {
			set addr [format "0x%x" [expr $gTransTablePinned + [expr $coid - $lTransTablePinned]]]
		} else {
			puts "What used a paged entry before the paged portion of\
			the local table has been setup"
			set addr [format "0x%x" [expr $gTransTablePaged + [expr $coid - $lTransTablePaged]]]
		}
		return [scalarResp [getField co $addr GTransEntry]]
		# Actually just p *(unsigned long *)$addr should do
	}

	# Move to gdb.tcl later
	proc parseRefs {type} {
		set fieldList [getTypeDefinition $type]
		foreach field $fieldList {
			if [regexp {[a-zA-Z]*:} $field ] {
				# protected, public, etc.
			} elseif [regexp {([a-zA-Z0-9]*) ([a-zA-Z0-9]*);} $field whole vartype var] {
				# What we want
				if [string match "*Ref" $vartype] {
					lappend retval "$vartype $var"
				}
			} elseif [regexp {.*?\(.*?} $field ] {
				# Function declarations have started
				break	
			}
		}
		set next [lindex $fieldList 0]
		if [regexp {.*?:(.*)\{} $next full match] {
			if [regexp {(public|protected|private) (.*)} $match full qualifier next] {
				set returned [parseRefs $next]
				if [string compare $returned {}] {
					if [info exists retval] {
						set retval [concat $retval $returned]
					} else {
						set retval $returned
					}
				}
			}
		}

		if [info exists retval] {
			return $retval
		}
	}

	# Prints all the refs from this root.
	proc getRefsFromRoot {root} {
		if {[string compare $root "0x0"]==0} {
			return
		}
		set type [lindex [getRootTypes $root] 0]
		set reflist [parseRefs $type]
		set rtnlist {}
		foreach item $reflist {
			set repref [scalarResp [getField [lindex $item 1] $root $type] numValue]
			lappend rtnlist $repref
		}
		foreach rep [getReps $root] {
			set rtnlist [concat $rtnlist [getRefs $rep]]
		}
	    return $rtnlist
	}

	# Prints all the refs from this rep.
	proc getRefsFromRep {rep} {
		if {[string compare $rep "0x0"]==0} {
			return
		}
		set typeList [isa $rep COSTransObject]
		set reflist [parseRefs [lindex $typeList 0]]
		set rtnlist {}
		foreach item $reflist {
			set repref [scalarResp [getField [lindex $item 1] $rep [lindex $typeList 0]] numValue]
			lappend rtnlist $repref
		}
	    return $rtnlist
	}
	
	# For now, cycles through root's reps and calls printInstance
	# TODO: Structure like getReps
	proc inspect {root} {
		set replist [getReps $root]
		foreach rep $replist {
			set type [getRepTypes $rep]
			if {[info procs ${type}_inspect] != ""} {
				${type}_inspect $rep
			} else {
				printInstance $rep [lindex $type 0]
			}
		}
	}

	# Useful only for isa. Strips leading <, trailing > and a space
	proc strip_extra_chars {str} {
		if [regexp {<?(.*)?> $} $str full match] {
			return $match
		} else {
			return $str
		}
	}

	proc isa {ptr type} {
		set retval ""
		set response [getInstance $ptr $type]
		set obj [split $response "\{=\}()"]
		set i 3
		while {[string match _vptr* [lindex $obj $i]]==0} {
			lappend retval [strip_extra_chars [lindex $obj $i]]
			set i [expr $i+2]
		}
		return $retval
	}

	proc getRootTypes {root} {
		return [isa $root COSMissHandlerBase]
	}

	proc getRepTypes {rep} {
		return [isa $rep COSTransObject]
	}
}

COSupport
