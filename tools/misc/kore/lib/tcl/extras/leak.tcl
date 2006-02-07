# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: leak.tcl,v 1.1 2005/08/19 23:15:50 bseshas Exp $


proc monitor_leaks {} {

	global snapshot

	proc gather {c r t} {
		return "$c $r $t"
	}

	proc toDisk {name filename} {
		set len [llength $name]
		set fp [open $filename {RDWR CREAT}]
		for {set i 0} {$i < $len} {incr i} {
			puts $fp [lindex $name $i]
		}
		close $fp
	}

	array unset snapshot

	set kount 0
	while {1} {
		if {[catch {set pass [processObjs .* gather]}]} {
			puts "Stopped"
			break
		}
		puts [llength $pass]
		set snapshot($kount) $pass
		incr kount
		after 1000
	}
	puts $kount
	for {set i 0} {$i<$kount} {incr i} {
		toDisk $snapshot($i) $i
	}
}
