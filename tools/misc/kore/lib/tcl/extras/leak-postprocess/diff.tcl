# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: diff.tcl,v 1.1 2005/08/19 23:16:47 bseshas Exp $


proc loadfile {filename} {
	set fp [open $filename {RDONLY}]
	while {1} {
		set str [gets $fp]
		if {[string compare $str ""]==0} {
			break
		}
		lappend retlist $str
	}
	close $fp
	return $retlist
}

proc diff {v1 v2 addns delns} {
	upvar 1 $addns a
	upvar 1 $delns d
	set i1 0
	set i2 0
	set a {}
	set d {}

	while {1} {
	if {[expr $i1 >= [llength $v1]] || [expr $i2 >= [llength $v2]]} {
		if {[expr $i1 < [llength $v1]]} {
			for {} {$i1 < [llength $v1]} {incr i1} {
				lappend d "[lindex $v1 $i1]"
				#puts "--- [lindex $v1 $i1]"
			}
		}
		if {[expr $i2 < [llength $v2]]} {
			for {} {$i2 < [llength $v2]} {incr i2} {
				lappend a "[lindex $v2 $i2]"
				#puts "+++ [lindex $v2 $i2]"
			}
		}
		return
	}

	if {[expr [regexp {(.*) (.*) (.*)} [lindex $v1 $i1] all c1 r1 t1] == 0]} { 
		puts [lindex $v1 $i1] 
		puts "Regexp error"
		return
	}
	if {[expr [regexp {(.*) (.*) (.*)} [lindex $v2 $i2] all c2 r2 t2] == 0]} { 
		puts [lindex $v2 $i2] 
		puts "Regexp error"
		return
	}

	set result [string compare $c1 $c2]

	if {[expr $result == 0]} {
		incr i1
		incr i2
	}
	if {[expr $result < 0]} {
		lappend d [lindex $v1 $i1]
		#puts "--- [lindex $v1 $i1]"
		incr i1
	}
	if {[expr $result > 0]} {
		lappend a [lindex $v2 $i2]
		#puts "+++ [lindex $v2 $i2]"
		incr i2
	}
	}
}

proc toHtml {histogram filename} {
	array set hist $histogram
	set MAXWIDTH 600
	set longest 0
	set biggest 0
	foreach key [array names hist] {
		set width $hist($key)
		if {[expr $width > $biggest]} {
			set biggest $width
		}
	}

	set fp [open $filename {RDWR CREAT}]
	puts $fp "<html><body>"
	foreach key [array names hist] {
		set color [unique_color]
		puts $fp "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"\#$color\">"
		puts $fp "<tr>"	
		set width [expr [expr $hist($key)*$MAXWIDTH]/$biggest]
		puts $fp "<td width=\"$width\">&nbsp;</td>"
		set width [expr $MAXWIDTH - $width]
		if {$width} {
			puts $fp "<td bgcolor=\"\#ffffff\" width=\"$width\"></td>"
		}
		puts $fp "<td bgcolor=\"\#ffffff\">&nbsp;&nbsp;&nbsp;$key \($hist($key)\)</td>"
		puts $fp "</tr>"	
		puts $fp "</table>"
	}
	puts $fp "</body></html>"
	close $fp
}

proc histogram {arr} {
	for {set i 0} {$i < [llength $arr]} {incr i} {
		set t [lindex [split [lindex $arr $i] " "] 2]
		if {[info exists hist($t)]} {
			incr hist($t)
		} else {
			set hist($t) 1
		}
	}

#	foreach key [array names hist] {
#		puts "$key\t\t\t$hist($key)"
#	}

	return [array get hist]
}

proc process {low high} {
	for {set i $low} {$i <= $high} {incr i} {
		set file($i) [loadfile $i]
	}

	set cum 0	
	# Pairwise diff
	for {set i $low} {$i < $high} {incr i} {
		diff $file($i) $file([expr $i+1]) a d
		histogram $a "addn.html"
		histogram $d "deln.html"
		set cum [expr $cum + [expr [llength $a] - [llength $d]]]
		puts "+ [llength $a] - [llength $d] ; $cum"
	}

}
