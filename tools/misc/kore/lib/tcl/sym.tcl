# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: sym.tcl,v 1.3 2005/08/19 23:11:20 bseshas Exp $

proc SymbolSupport {} {

	proc setupSymbol {} {
		global symTbl symCnt
		set symCnt 0
		return 1
	}

	proc loadSymTbl {symFile} {
		global symTbl symCnt
		set i 0
		if {[catch \
			 {set fd [open "|nm -a -C $symFile" r]} ]} {
			puts "Error loading Symbols from $symFile"
			return 0
		}
				
		puts -nonewline "Loading Symbols from $symFile:"
		flush stdout
		while {[gets $fd line] >= 0} {
			if {[regexp {(.*) V vtable for (.*)} $line all add name] == 1} {
				set symTbl(0x$add) $name
				set symCnt [expr $symCnt + 1]
				set i [expr $i + 1]
				if { $i == 50 } {
					puts -nonewline "."
					flush stdout
					set i 0
				}
			}
		}
		puts ""
		close $fd
	}
			
	proc getType { typeToken } {
		global symTbl
		set index [format "0x%lx" [expr wide($typeToken) - 16]]
		if [info exists symTbl($index)] {
			return $symTbl($index)
		} else {
			return UNKNOWN
		}
	}

	setupSymbol
}

SymbolSupport
