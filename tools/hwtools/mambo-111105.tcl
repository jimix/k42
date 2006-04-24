#! mambo-apache -q
# -*-tcl-*-
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: mambo-111105.tcl,v 1.2 2005/11/15 17:17:32 butrico Exp $
# ############################################################################
# This file is specifically for mambo
#
# There are some tcl variable that control the behavior of this file
# k42_skip_k42_tcl, if set,  skips the k42.tcl file
# k42_skip_utils_tcl, if set,  skips the utils.tcl file
# The external (enviroment) variable MAMBO_EXTRA_TCL, if set, should contain
# a valid tcl statement, e. g.,
#
# source my_tcl_file
#
# my_tcl_file would be a good place where the other tcl variables can be
# set.  If a tcl file sourced via the MAMBO_EXTRA_TCL mechanism defines the
# proc mambo_start_proc, then k42_start_proc procedure is invoked instead of 
# instantiating a simulator. The procedure is invoked after k42.tcl and 
# utils.tcl are sourced.  
#
# See also k42.tcl and utils.tcl

if { [info exists env(MAMBO_EXTRA_TCL)] } {
    puts "eval $env(MAMBO_EXTRA_TCL)"
    eval $env(MAMBO_EXTRA_TCL)
} else {
    puts "no MAMBO_EXTRA_TCL found."
}

if { [lsearch [display configures] cfg]==-1 } {
   # cfg has not be created as a configuration yet so go ahead and follow
   # our default conventions.
   if [info exists env(MAMBO_TYPE)] {    
       define dup $env(MAMBO_TYPE) cfg
   } else {
       define dup gpul cfg
   }
}

if { [ info exists env(MAMBO_NUM_CPUS) ] } {
    cfg config cpus $env(MAMBO_NUM_CPUS)
}

if { [ info exists env(MAMBO_ROM_FILE) ] } {
    cfg config mcm 0 ROM_file $env(MAMBO_ROM_FILE)
    #mysim mcm 0 config  ROM_file $env(MAMBO_ROM_FILE)
}


if { ! [info exists mambo_no_memory_config] } {
  if { [catch [cfg config memory_size "$env(MAMBO_MEM)M"]] } {}
}


if { [lsearch [display machines] mysim]==-1 } {
   # mysim has not be created as a simulation yet so go ahead and follow
   # our default conventions.
    define machine cfg mysim
}

mysim debugger listen

#if { ! [info exists mambo_no_memory_config] } {
#  if { [catch [mysim modify memory_file $env(MAMBO_GARB_FNAME)]] } {}
#}

if { ! [info exists mambo_no_debug] && 
     [info exists env(MAMBO_DEBUG_PORT) ] } {
    if { [catch [mysim modify gdb_port $env(MAMBO_DEBUG_PORT)]] } {}
}

puts "Setting thinwire port to $env(MAMBO_SIMULATOR_PORT) ..."	     
mysim modify thinwire_port $env(MAMBO_SIMULATOR_PORT);

# display clock, instructions and cycles
mysim modify console_format 7


# Load the image
if { [info exists env(MAMBO_BOOT_FILE) ] } {
    if { [info exists env(MAMBO_BOOT_VMLINUX) ] } {
	puts "Loading a K42 boot image of the vmlinux style ..."
	mysim load vmlinux $env(MAMBO_BOOT_FILE) 0
    } else {
	mysim load elf $env(MAMBO_BOOT_FILE)
    }
} else {
    mysim load elf "mamboboot.tok"
}

set mamborc /dev/null
if { [info exists env(HOME) ] &&
     [ file exists $env(HOME)/.mamborc ] } {
    set mamborc $env(HOME)/.mamborc
}
source $mamborc


if { ! [info exists mambo_skip_os_tcl] &&
     [ info exists env(MAMBO_OS_TCL) ]} {
    source $env(MAMBO_OS_TCL)
}

if { ! [info exists mambo_skip_utils_tcl] &&
     [ info exists env(MAMBO_UTIL_TCL) ] } {
    source $env(MAMBO_UTIL_TCL)
}

if { [info procs mambo_start_proc]=="mambo_start_proc" } {
    puts "k42_start_proc"
    k42_start_proc mysim
} else {
    puts "config fast"
    mysim modify fast on
    
    if { [info exists env(MAMBO_EARLY_GDB)] &&
	 $env(MAMBO_EARLY_GDB) == "1" } {
	mysim modify fast off
	mysim debugger wait 0
    }

    if { ! [info exists env(MAMBO_NO_GO)] ||
	 $env(MAMBO_NO_GO) != "1" } {
	mysim go
    }
}


# Allows for an environment variable in regress to force mambo to exit
if { [info exists env(MAMBO_EXIT)] && 
     $env(MAMBO_EXIT) == "yes" } {
    exit 0
}
