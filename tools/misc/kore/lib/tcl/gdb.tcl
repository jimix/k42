# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: gdb.tcl,v 1.3 2005/08/19 23:10:04 bseshas Exp $

proc GDBSupport {} {
	proc setupGDB {} {
		global gdbFD gdbPath gdbImageFile env

		set gdbImageFile "$env(KERNEL_IMAGE)"
		set gdbPath "gdb"

		set gdbFD [open "|$gdbPath -n -q -f 2>&1 | cat" r+]

		fconfigure $gdbFD -buffering line -translation binary
		#	fconfigure $gdbFD -buffering none
		initGDB
		initKludges
		return 1
	}

	proc initGDB {} {
		global gdbImageFile gdbFD
		
		# The protcol thes program uses to talk to gdb is that all responses
		# from gdb end with "gdb-done\n" to get this primed we set the prompt
		# and read the response by hand to clear the input buffer.  
		puts $gdbFD {set prompt gdb-done\n}
		gets $gdbFD
		
		# from this point on all interactions should occur via sendToGDB
		sendToGDB "set confirm off"
		sendToGDB "set pagination off"
		sendToGDB "set print object on"
		sendToGDB "set print demangle on"
		sendToGDB "set print asm-demangle on"
		sendToGDB "set print static-members on"
		sendToGDB "set print vtbl on"
		sendToGDB "set output-radix 16"
		loadGDBInitExecutable $gdbImageFile
	}

	proc initKludges {} {
		sendToGDB "ptype PMRoot::MyRoot"
		sendToGDB "ptype HWPerfMon::HWPerfMonRoot"
		sendToGDB "ptype SystemMisc::SystemMiscRoot"
	}

	proc gdbConnect {host} {
		sendToOIS "getInspectPort"
		set port [getOISResponse]
		puts "Connecting gdb to $host $port\n";
		gdbTargetHost $host $port
	}

	proc gdbTargetHost {host port} {
		return [sendToGDB "target remote $host:$port"]
	}

	proc addGDBSymbolFile { file } {
		sendToGDB "add-symbol-file $file"
	} 

	proc loadGDBInitExecutable { file } {
		sendToGDB "file $file"
	}
	
	proc getGDBResponse {} {
		global gdbFD
		set line ""
		set response {}
		
		while { [gets $gdbFD line] >= 0 } {
			if { $line =="gdb-done" } {
				#			puts "found gdb respone termination: $line"
				return $response
			} else {
				#			puts "line: $line"	
				lappend response $line
			}
		}
		puts "Error: getGDBResponse failed"
		return {}
	}

	proc LHS { res sep } { 
		if {[regexp (.*?)${sep}(.*) $res m l r]} {
			return $l
		}
	}

	# Returns what follows sep in res
	proc RHS { res sep } { 
		if {[regsub .*?$sep $res {} result]} {
			return $result
		}
	}

	proc hexValue { rhs } {
		if {[regexp {.*(0x[0-9a-f]*).*} $rhs m v]} {
			return $v
		}
	}

	# Matches the hex/decimal value found at the beginning of the string
	proc numValue { rhs } {
		if {[regexp {(0x[0-9a-f]*|[0-9]*)?.*} $rhs m v]} {
			return $v
		}
	}

	# Extracts the first string value (enclosed in double quotes)
	proc strValue { rhs } {
		if {[regexp {\"(.*)?\".*} $rhs str v]} {
			return $v
		}
	}

	# Extracts the first alphanumeric value (enclosed in double quotes)
	proc alphaNum { rhs } {
		if {[regexp {([0-9a-zA-Z]*)?.*} $rhs str v]} {
			return $v
		}
	}

	proc scalarResp {res {valType hexValue}} {
		return [$valType [RHS [lindex $res 0] "= "]]
	}

	proc sendToGDB { cmd } {
		global gdbFD
		#	puts "sending : $cmd"
		puts -nonewline $gdbFD "$cmd\n"
		set retval [getGDBResponse]
		return $retval
	}

	proc printGDBResponse {response} {
		puts [join $response "\n"]
	}
	
	proc getTypeDefinition { type } {
		return [sendToGDB "ptype struct '$type'"]
	}

	proc printTypeDefinition { type } {
		sendToGDB "set print pretty on"
		puts [sendToGDB "ptype struct '$type'"]
		sendToGDB "set print pretty off"
	}

	proc getInstance {ptr type} {
		set response [sendToGDB "p *(struct '$type'  *)$ptr"]
		return $response
	}

	proc printInstance {ptr type} {
		sendToGDB "set print pretty on"
		printGDBResponse [getInstance $ptr $type]
		sendToGDB "set print pretty off"
	}

	proc getField {field ptr type} {
		return [sendToGDB "p ((struct '$type' *)$ptr)->$field"]
	}

	setupGDB
}

GDBSupport
