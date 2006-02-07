# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: ois.tcl,v 1.3 2005/08/19 23:11:20 bseshas Exp $

proc oisSupport {} {
	proc setupOISSupport {} {
		global oisHost oisPort oisFD env
		set oisHost $env(MACHINE)
		set oisPort 3624
		return 1		
	}

	proc openOISConnection {} {
		global oisHost oisPort oisFD
		if {[catch { set oisFD [ socket $oisHost $oisPort ]} ]} {
			return -1
		}
		fconfigure $oisFD -buffering line -translation binary  
		return $oisFD
	}

	proc closeOISConnection {} {
		global oisFD
		close $oisFD
	}

	proc sendToOIS {oisMsg} {
		global oisFD
		puts $oisFD "$oisMsg" 
		flush $oisFD
	}

	# read 1 byte from OIS, convert to ascii integer and return
	proc getOISHeader {} {
		global oisFD
		set header [read $oisFD 8]
		binary scan $header W retval
		return $retval
	}

	# read n bytes from OIS and return
	proc getOISData {n} {
		global oisFD
		set data [read $oisFD $n]
		return $data
	}

	proc getOISResponseLine {} {
		global oisFD
		if {[gets $oisFD line] >=0 } {
			if { $line != "ois-done" } {
				return $line
			}
		} else {
			puts "Error: getOISResponseLine failed"
			close $oisFD
		}
		return ""
	}

	proc getOISResponse {} {
		global oisFD
		set line ""
		set response {}
		
		while { [gets $oisFD line] >= 0 } {
			if { $line =="ois-done" } {
				return $response
			} else {
				#puts "line: $line"	
				lappend response $line
			}
		}
		close $oisFD
		puts "Error: getOISResponse failed"
		return {}
	}
	
	setupOISSupport
}

oisSupport
