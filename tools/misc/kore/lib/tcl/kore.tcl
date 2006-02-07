# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: kore.tcl,v 1.4 2005/08/19 23:02:18 bseshas Exp $

proc KoreSupport {} {
    global env

    if [info exists env(KORE_LIBDIR)] {
        global korePath
        set korePath "$env(KORE_LIBDIR)/tcl"
    	lappend korePath "$env(KORE_LIBDIR)/tcl/extras"
    }

    if ![info exists korePath] {
        global korePath
        set korePath "."
    }
    lappend korePath ~/.kore

    puts "KORE: korePath Initialized to : $korePath"

    proc koreSource {file} {
        global korePath
        if {[file readable $file]} {
#            puts "KORE: *** sourcing $file\n"
            source $file
        } else {
#            puts "KORE: searching $korePath for $file\n"
            foreach p $korePath {
#                puts "KORE: checking $p for $file"
                if [file readable "$p/$file"] {
#                    puts "KORE: found $file in $p"
                    return [source "$p/$file"]
                }
            }
        }
        puts "KORE: *** $file not found (korePath = $korePath)"
        return -1
    }
    proc koreStartCmdLine {} {
	    if {[catch {package require tclreadline}]} {
			puts "WARNING: tclreadline not found in TCLLIBPATH."
			# Default very primitive shell w/ no readline support
			uplevel #0 {
				while {1} {
					puts -nonewline "kore> "
					flush stdout
					gets stdin LINE
					eval $LINE
				}
			}
		} else {
			::tclreadline::Loop
		}
    }
}

proc help {procname} {
	global helpDB
	if [info exists helpDB($procname)] {
		puts $helpDB($procname)
	} else {
		puts "Help on $procname not found"
	}
}

proc reconnect {} {
	global env
	catch {closeOISConnection}
	openOISConnection
	gdbConnect $env(MACHINE)
}

global env loadSymTbl

KoreSupport

koreSource sym.tcl
koreSource gdb.tcl
koreSource ois.tcl
koreSource obj.tcl
koreSource gui.tcl
koreSource cobj.tcl
loadSymTbl $env(KERNEL_IMAGE)
openOISConnection
gdbConnect $env(MACHINE)
puts "Ready"

koreStartCmdLine
