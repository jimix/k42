# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: map2html.tcl,v 1.1 2005/08/19 23:17:20 bseshas Exp $


proc map2html {filename htmlname} {
	set fp [open $filename RDONLY]
	set html [open $htmlname {RDWR CREAT}]
	puts $html "<html><body><table cellpadding=\"0\" cellspacing=\"0\" border=\"0\">"
	set kount 0
	set pgkount 0
	set addr -4096
	set processlist {}
	while {1} {
		if {[expr [expr $kount % 64] == 0]} {
			puts $html "<tr>"
			puts $html "<td bgcolor=\"#ffffff\">0x[format %x [expr $addr+0x1000]]</td>"
		}
		gets $fp line
		if {[eof $fp]} {
			break
		}
		if {[regexp {(.*)\t(.*)\t(.*)} $line all addr process attr]} {
			if {[string compare $process ""]} {
				if {[expr [lsearch $processlist $process] == -1]} {
					lappend processlist $process
					set color($process) [unique_color]
					set alpha($process) [unique_alphabet]
				}
				puts -nonewline $html "<td bgcolor=\"$color($process)\">$alpha($process)</td>"
			} else {
				puts -nonewline $html "<td bgcolor=\"#ff9999\">X</td>"
			}
		} elseif {[regexp {(.*)\t(.*)} $line all addr attr]} {
			if {[string compare $attr UNKNOWN]} {
				puts -nonewline $html "<td bgcolor=\"#99ff66\">O</td>"
			} else {
				puts -nonewline $html "<td bgcolor=\"#aaaaaa\">?</td>"
			}
		}
		if {[expr [expr $kount % 64] == 63]} {
		# Next line
			puts $html "</tr>"
			if {[expr [expr $kount % 65536] == 65535]} {
			# Next page
				puts $html "</table>"
				puts $html "<a href=\"$pgkount$htmlname\"> Next </a>"
				puts $html "</body></html>"
				close $html
				set html [open $pgkount$htmlname {RDWR CREAT}]
				puts $html "<html><body><table cellpadding=\"0\" cellspacing=\"0\" border=\"0\">"
				incr pgkount
			}
		}
		incr kount
	}
	puts $html "</table></body></html>"
	close $fp
	close $html

	set html [open "process.html" {RDWR CREAT}]
	puts $html "<html><body><table cellpadding=\"0\" cellspacing=\"0\" border=\"0\">"
	for {set i 0} {$i < [llength $processlist]} {incr i} {
		puts $html "<tr>"
		puts $html "<td bgcolor=\"$color([lindex $processlist $i])\"> $alpha([lindex $processlist $i]) </td>"
		puts $html "<td bgcolor=\"#ffffff\"> [lindex $processlist $i] </td>"
		puts $html "</tr>"
	}
	puts $html "</table></body></html>"
	close $html
}
