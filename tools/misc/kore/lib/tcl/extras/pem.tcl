# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: pem.tcl,v 1.1 2005/08/19 23:16:22 bseshas Exp $


proc PemSupport {} {

	proc getDiffObjects {} {
		sendToOIS getDiffObjects
		set data {}

		while {1} {
			set size [getOISHeader]
			puts $size
			if {[expr $size == 9]} {
				set done [getOISData $size]
				if {[expr [string compare $done "ois-done"] == 0]} {
					return $data			
				} else {
					append data $done
					continue
				}
			}
			while {[expr $size > 1024]} {
				puts -nonewline .
				flush stdout
				append data [getOISData 1024]
				set size [expr $size - 1024]
			}
			append data [getOISData $size]
		}
	}

	proc getDiffObjectsOldProtocol {} {
		sendToOIS getDiffObjects
		set response [getOISResponse]
		set len [llength $response]
		set packet {}
		for {set i 0} {$i < [expr $len-1]} {incr i} {
			append packet "[lindex $response $i]\n"
		}
		append packet [lindex $response [expr $len-1]]
		binary scan $packet W* trace
		return $trace
	}

	proc walkTrace {trace} {
		set len [llength $trace]
		set ptr 0
		while {[expr $ptr <= $len]} {
			set hexval 0x[format %x [lindex $trace $ptr]]
			set timeStamp [expr $hexval >> 32]
			puts "Timestamp $timeStamp"
			set eventLen [expr [expr 0xff000000 & $hexval] >> 24]
			puts "Length $eventLen"
			set majorID [expr [expr 0xfc000 & $hexval] >> 14]
			puts "MajorID $majorID"
			set eventType [expr 0x3fff & $hexval]
			puts "EventType $eventType"
			if {[expr $eventLen == 0]} {
				incr eventLen
			}
			set ptr [expr $ptr + $eventLen]
		}
	}
}

PemSupport
