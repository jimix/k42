# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: obj.tcl,v 1.4 2005/10/18 00:00:08 jappavoo Exp $

proc BasicObjSupport {} {

	proc setupBasicObjSupport {} {
		global host port first_time env
		set host $env(MACHINE)
		set port 3624
		set first_time 1
		return 1
	}

	proc openObjectStream {} {
 #	   if { [openOISConnection] == -1 } {
 #		   return 0
 #	   }
		sendToOIS "getObjectList" 
		return 1
	}

	proc getObject {} {
		set obj {}
		if {[set line [getOISResponseLine]] != ""} {
			set obj [split $line]
			set obj [lreplace $obj 2 2 [getType [lindex $obj 2]]]
		}
		return $obj
	}
	
	proc closeObjectStream {} {
		closeOISConnection
	}

	proc doPeriodic {time func} {
		$func
		after $time doPeriodic $time $func
	}

	proc prettyPrint {c r t} {
		puts "coid=$c root=$r type=$t"
		return "$c $r $t"
	}
       
	proc processObjs {filter func {limit 9999}} {
		set kount 0
		set retval {}
		if [openObjectStream] {
			while { [set obj [getObject]] != {} } {
				if { [regexp $filter $obj] } {
					set coid [lindex $obj 0]
					set root [lindex $obj 1]
					set type [lindex $obj 2]
					set ans [$func $coid $root $type]
					if {[string compare $ans ""]} {
						lappend retval $ans
					}
					set kount [expr $kount+1]
				}
				if {[expr $kount == $limit]} {
					while {[getObject] != {}} {
					# flush the remaining
					}
					break
				}
			}
		}
		#puts "\n$kount entities" 
		if [info exists retval] {
			return $retval
		}
	}

	set helpDB(printObjs) "Prints the coid, root ptr and type of all currently \
	allocated objects"

        proc rtnObj {c r t} { return "coid=$c root=$r type=$t" }

	proc printObjs {filter} {
		if [openObjectStream] {
			while { [set obj [getObject]] != {} } {
				if { [regexp $filter $obj] } {
					set coid [lindex $obj 0]
					set root [lindex $obj 1]
					set type [lindex $obj 2]
					puts "coid=$coid root=$root type=$type"
				}
			}
		}
	}

	proc onCreate {type func} {
		puts "onCreate $type $func"
	}

	proc onDestroy {type func} {
		puts "onDestroy $type $func"
	}

	proc hotSwap {fac root} {
		sendToOIS "hotSwapInstance,$fac,$root"
	} 

	proc takeOver {newFac oldFac} {
		sendToOIS "takeOverFromFac,$newFac,$oldFac"
	}

	setupBasicObjSupport
}

BasicObjSupport
