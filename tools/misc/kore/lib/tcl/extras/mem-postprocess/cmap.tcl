# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: cmap.tcl,v 1.1 2005/08/19 23:17:20 bseshas Exp $


proc CreateMap {} {
	global fcmlist rlist fcm2fr fcm2pm r2fcm r2p map paddrlist freelist

	# Loads the list of free pages	
	proc LoadFreeList {filename} {
		global freelist

		set fp [open $filename RDONLY]
		while {1}  {
			gets $fp line
			if [eof $fp] {
				break
			}
			set parsed [split $line " ="]
			set paddr [lindex $parsed 0]
			set size [lindex $parsed 5]
			if {[expr [regexp {0xc0*(.*)} $paddr parsed paddr] != 1]} {
				puts "Error $parsed $paddr"
			}
			#puts "0x$paddr $size"
			set paddr 0x$paddr	

			while {[expr $size > 0]} {
				lappend freelist $paddr
				set size [expr $size - 0x1000]
				set paddr [expr $paddr + 0x1000]
			}
			puts -nonewline "."
		}
		close $fp
	}

	# Loads the vectors - fcmlist, rlist, fcm2fr, fcm2pm, r2fcm, r2pm
	proc LoadList {filename} {
		set retlist {}
		set fp [open $filename RDONLY]
		while {1} {
			gets $fp line
			if [eof $fp] {
				break
			}
			lappend retlist $line
		}
		close $fp
		return $retlist
	}

	foreach item {fcmlist rlist fcm2fr fcm2pm r2fcm r2p} {
		set $item [LoadList $item]
	}	

	puts "Loaded files"

	LoadFreeList freepages

	puts "Loaded free list"

	set fp [open map RDONLY]
	while {1} {
		gets $fp line
		if [eof $fp] {
			break
		}
		regexp {paddr ([x0-9abcdef]*), fileOffset ([x0-9abcdef]*), used ([x0-1]*), dirty ([x0-1]*), mapped ([x0-1]*), free ([x0-1]*), cacheSynced ([x0-1]*), length ([x0-9abcdef]*), FCM (.*) (.*) (.*)} $line str paddr fOff used dirty mapped free cache len c r t
		set map($paddr) "$paddr $fOff $used $dirty $mapped $free $cache $len $c $r $t"
		lappend paddrlist $paddr
		puts -nonewline .
	}
	close $fp

	puts "Loaded FCM maps"

	set paddrlist [lsort -integer $paddrlist]
	set freelist [lsort -integer $freelist]
	puts [lindex $paddrlist [expr [llength $paddrlist]-1]]
	set max [expr [lindex $paddrlist [expr [llength $paddrlist]-1]]/4096]
	if {[expr [expr [lindex $freelist [expr [llength $freelist]-1]]/4096] > $max]} {
		set max [expr [lindex $freelist [expr [llength $freelist]-1]]/4096]
	}

    set fp [open map_test_processed {RDWR CREAT}]
	set pindex 0
	set findex 0
	set plen [llength $paddrlist]
	set flen [llength $freelist]
	for {set i 0} {$i <= $max} {incr i} {
		set addr 0x[format %x [expr $i * 4096]]
		if {[expr $pindex < $plen]} {
		if {[expr [lindex $paddrlist $pindex] == $addr]} {
			# match
			set fcm [lindex $map($addr) 9]
			set fcmindex [lsearch $r2fcm $fcm]
			set process [lindex $r2p $fcmindex]
			puts $fp "$addr\t$process\t$map($addr)"
			incr pindex
			continue
		}
		} 
		if {[expr $findex < $flen]} {
		if {[expr [lindex $freelist $findex] == $addr]} {
			puts $fp "$addr\tFREE"
			incr findex
			continue
		}
		}
			puts $fp "$addr\tUNKNOWN"
	}
	close $fp
}
