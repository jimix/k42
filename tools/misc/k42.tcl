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
#  $Id: k42.tcl,v 1.11 2005/10/25 20:54:22 apw Exp $
# ############################################################################
#
# There are some tcl variable that control the behavior of this file
# k42_no_debug, if set,  suppresses the configuration of a gdb_port and a few
#	other related values;
# k42_no_memory_config, if set, suppresses the configuration of mambo memory
#	and memory file;

proc k42_getenv { name_addr name_size result_addr } {
    global env

    # reads the name from $name_addr and puts it in $var_name
    set naddr [mysim util itranslate $name_addr]
    set var_name ""
    for {set i 0} {$i < $name_size} {incr i} {
	set var_name [format "%s%c" $var_name [ mysim memory display $naddr char ]]
	incr naddr
    }

    puts -nonewline "k42_getenv( $var_name ): "

    if [info exists env($var_name)] {
	set result_str $env($var_name)
	puts "$env($var_name)"
    } else {
	set result_str ""
	puts "(empty)"
    }

    # stuffs $env($var_name) in the $result_addr
    set raddr [mysim util itranslate $result_addr]
    set result_length [string length $result_str]
    for {set i 0} {$i < $result_length} {incr i} {
	binary scan [string index $result_str $i] c1 v
	mysim memory set $raddr 1 $v
	incr raddr
    }
    # null-termination
    mysim memory set $raddr 1 0
}

proc k42_getkparms_size { result_addr } {
    global env

    set filesize [ file size $env("K42_KPARMS_FILE") ]
    set raddr [mysim util itranslate $result_addr]
    mysim memory set $raddr 8 $filesize
    puts "kparm.data is of size $filesize"
}

proc k42_getkparms { result_addr } {
    global env

    set filesize [ file size $env("K42_KPARMS_FILE") ]
    set raddr [mysim util itranslate $result_addr]
    mysim memory fread $raddr $filesize $env("K42_KPARMS_FILE")
}

#
# Uncomment one of these to help you debug faults
#
#mysim trigger set pc 0x300 "dpage_fault mysim"
#mysim trigger set pc 0x380 "dpage_fault mysim" 
#mysim trigger set pc 0x400 "ipage_fault mysim"
#mysim trigger set pc 0x480 "ipage_fault mysim"
#mysim trigger set pc 0x500 "just_stop mysim"
#mysim trigger set pc 0x980 "hdec"
#mysim trigger set pc 0xc00 "hcall"

# trick linux that this is a POWER4 so it can do SLB stuff
#mysim cpu 0 set spr pvr 0x350000

# until mambo defines a new cpu id for percs, force BE id - mambo
# uses gpul which is wrong for K42
if { [info exists env(MAMBO_TYPE)] && 
   $env(MAMBO_TYPE) == "percs" } {
	mysim cpu 0 set spr pvr 0x700100
}

#configuring bogus disks
if [info exists env(MAMBO_KFS_DISK_KITCHROOT)] {
   mysim bogus disk init 0 $env(MAMBO_KFS_DISK_KITCHROOT) cow $env(MAMBO_KFS_DISK_KITCHROOT).cow 1024
}
if [info exists env(MAMBO_KFS_DISK_ROOT)] {
   mysim bogus disk init 1 $env(MAMBO_KFS_DISK_ROOT) cow $env(MAMBO_KFS_DISK_ROOT).cow 1024
}
if [info exists env(MAMBO_KFS_DISK_EXTRA)] {
   mysim bogus disk init 2 $env(MAMBO_KFS_DISK_EXTRA) cow $env(MAMBO_KFS_DISK_EXTRA).cow 1024
}
if [info exists env(MAMBO_EXT2_DISK_EXTRA)] {
   mysim bogus disk init 3 $env(MAMBO_EXT2_DISK_EXTRA) cow $env(MAMBO_EXT2_DISK_EXTRA).cow 1024
   puts "Initialized bogus device 3 to $env(MAMBO_EXT2_DISK_EXTRA)"
}

# This is necessary to allow gdb to function (set breakpoints)
if { ! [info exists k42_no_debug]  } {
  puts "To debug mambo with gdb: attach [pid]"
}



if { [info exists env(MAMBO_TYPE)] && 
   $env(MAMBO_TYPE) == "percs" && 
   [info exists env(MAMBO_PERCS_TCL)] } {
    source $env(MAMBO_PERCS_TCL)
} else {
    proc k42_isMipOn { result_addr } {
	# put the return info in the provided address
	set raddr [mysim util itranslate $result_addr]
	mysim memory set $raddr 8 0
	puts "k42_isMipOn == 0"
    }
}

# Callback to start ztracing
proc start_ztrace { sim fname args } {
    mysim modify fast off
    simstop

    puts ""
    puts "++++ Type `mysim go' to begin tracing ++++"
    puts ""

    puts "Turned off fast mode, turning on the reader ..."
    ereader expect 1
    global env

    if { $env(MAMBO_ZVAL) == "addrHistoEmitterReader" } {
	ereader start $env(MAMBO_ZVAL) [pid] $env(MAMBO_ZVAL_FILE)
    } else {
	ereader start $env(MAMBO_ZVAL) [pid] $env(MAMBO_ZVAL_FILE) 0 0 0
    }
}

# Callback to stop ztracing
proc stop_ztrace { sim fname args } {
    puts "Stopping tracing and exiting ..."
    simstop
    quit
}

# If we are going to ztrace, set up the triggers and emitter
if { [info exists env(MAMBO_ZVAL)] } {
    puts "Setting up triggers for ztrace run ..."
    mysim trigger set console $env(MAMBO_ZVAL_START) start_ztrace
    mysim trigger set console $env(MAMBO_ZVAL_STOP) stop_ztrace
    
    puts "Turning on the emitter ..."
    simemit set "Header_Record" 1
    simemit set "Footer_Record" 1
    simemit set "Instructions" 1
    simemit set "Memory_Write" 1
    simemit set "Memory_Read" 1
    simemit set "L1_DCache_Miss" 1
    simemit set "L2_Cache_Miss" 1
    simemit set "L2_Cache_Hit" 1
    simemit set "Config" 1
    simemit set "Start_Stats" 1
    simemit set "Stop_Stats" 1
    simemit set "MSR" 1
    simemit set "Pid_Resumed" 1
    simemit set "Pid_Exited" 1
    simemit start
}
