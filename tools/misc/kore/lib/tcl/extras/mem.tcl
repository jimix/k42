# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: mem.tcl,v 1.2 2005/08/20 22:56:07 bseshas Exp $
# ############################################################################

# expects GDBSupport

proc MemSupport {} {
	global helpDB

	set helpDB(exploreFCM) "Prints some fields from given FCM, updates \
	fcm2fr and fcm2pm mappings and returns the list of FCMs"
	proc exploreFCM {c r {t FCMComputation}} {
		global fcm2fr fcm2pm fcmlist FP
		
		set count 0
		set response [getReps $r]
		if {[string compare $response "0x0"] == 0} {
			puts "NULL"
			return
		}
		set response [getInstance $response $t]
		set frRef [numValue [RHS $response "frRef = "]]
		set pmRef [numValue [RHS $response "pmRef = "]]
		if {[string compare $frRef ""]==0 || [string compare $pmRef ""]==0} {
			return
		}
		set frresponse [sendToGDB "p (**(FRRef)$frRef)->myRoot"]
		set pmresponse [sendToGDB "p (**(PMRef)$pmRef)->myRoot"]
		set fcm2fr($r) [numValue [RHS $frresponse "\[\\\)\]\[\\\ \]"]]
		set fcm2pm($r) [numValue [RHS $pmresponse "\[\\\)\]\[\\\ \]"]]
		lappend fcmlist $r
		set hashTable [numValue [RHS $response "hashTable = "]]
		set hashMask [numValue [RHS $response "hashMask = "]]
		set firstIndexUsed [numValue [RHS $response "firstIndexUsed = "]]
		set total [numValue [RHS $response "numPages = "]]
		sendToGDB "set print object off"

# If print object is NOT set to off, kernel crashes with:
#/homes/kix/bseshas/k42/kitchsrc/os/kernel/proc/ProcessShared.C,247: Invalid memory access: processID 0x0 addr 0x0, type 1073741824
#vector=0x300, sr=0x900000000000b032, pc=0xc000000002159450 lr=0xc0000000023bea80
		for {set i $firstIndexUsed} {$i<=$hashMask} {incr i} {
			set response [sendToGDB "p (('PageSet<AllocGlobal>::Entry' **)$hashTable)\[$i\]"]
			set addr [numValue [RHS $response "\[\\\)\]\[\\\ \]"]]
			if {[string compare $addr "0x0"]} {
				while {[string compare $addr "0x0"] && [string compare $addr ""]} {
				# TODO: Could be optimized
					set response [sendToGDB "p *(('PageSet<AllocGlobal>::Entry' *)$addr)"]
				#	puts "paddr [numValue [RHS $response "paddr = "]], fileOffset [numValue [RHS $response "fileOffset = "]], used [numValue [RHS $response "used = "]], dirty [numValue [RHS $response "dirty = "]], mapped [numValue [RHS $response "mapped = "]], free [numValue [RHS $response "free = "]], cacheSynced [numValue [RHS $response "cacheSynced = "]], length [numValue [RHS $response "len = "]]"
					puts $FP "paddr [numValue [RHS $response "paddr = "]], fileOffset [numValue [RHS $response "fileOffset = "]], used [numValue [RHS $response "used = "]], dirty [numValue [RHS $response "dirty = "]], mapped [numValue [RHS $response "mapped = "]], free [numValue [RHS $response "free = "]], cacheSynced [numValue [RHS $response "cacheSynced = "]], length [numValue [RHS $response "len = "]], FCM $c $r $t"
					set addr [numValue [RHS $response "next = "]] 
					incr count
				}
			} else {
			}
			if {[expr $count >= $total]} {
				sendToGDB "set print object on"
				return {}
			}
		}
				sendToGDB "set print object on"
				return {}
	}

	set helpDB(FCMPages) "Prints number of pages in FCM \$root"
	proc FCMPages {coid root type} {
		puts -nonewline "$root $type "
		set response [sendToGDB "p ((struct $type *)((struct CObjRootSingleRep *)$root)->therep)->pageList.numPages"]
		set obj [split $response "\{ =\}"]
		puts [lindex $obj 4]
	}

	set helpDB(listNumPages) "processObjs iterator for FCMPages"
	proc listNumPages {filter {limit 999999}} {
		processObjs $filter FCMPages $limit
	}

	set helpDB(FCMSinPM) "Prints all the FCMs associated with given PMLeaf"
	proc FCMSinPM {root {type PMLeaf}} {
		set response [getInstance [getReps $root] $type]
		set numFCM [numValue [RHS $response "numFCM = "]]
		puts "$numFCM FCM's found"
	}

	proc explorePMRoot {coid root {type PMRoot::MyRoot}} {
		set rep [getReps $root]
		set response [getField freeFrameList $rep PMRoot]
		set head [numValue [RHS $response "head = "]]
		set tail [numValue [RHS $response "tail = "]]
		set count [numValue [RHS $response "count = "]]
		set ptr $head
		while {[expr [string compare $ptr $tail] && [string compare $ptr 0x0]]} {
			exploreFree $ptr
			set ptr [scalarResp [getField next $ptr FreeFrameList::FrameInfo]]
		}
		if {[string compare $ptr 0x0]} {
			exploreFree $ptr
		}
		return {}
	}

	proc exploreFree {ptr} {
		global pmroot

		set frameCount [scalarResp [getField frameCount $ptr FreeFrameList::FrameInfo]]
		if {[expr $frameCount > 509]} {
			puts "WARNING: frameCount ($frameCount) > 509"
		}
		lappend pmroot "FrameCount $frameCount"
		for {set i 0} {$i<$frameCount} {incr i} {
			set response [scalarResp [sendToGDB "p (('FreeFrameList::FrameInfo' *)$ptr)->pages\[$i\]"]]
			lappend pmroot "$response"
		}
	}

	proc printTree {ptr} {
		global pa

		set response [getInstance $ptr PageAllocatorDefault::freePages]
		set size [numValue [RHS $response "size = "]]
		if {[expr $size != 0x0]} {
		#	puts "[numValue [RHS $response "start = "]]  size = $size"
			lappend pa "[numValue [RHS $response "start = "]]  size = $size"
		}
		set low [numValue [RHS $response "low = "]]
		set high [numValue [RHS $response "high = "]]
		if {[expr $low != 0x0]} {
			printTree $low
		} 
		if {[expr $high != 0x0]} {
			printTree $high
		}
	}

	set helpDB(explorePageAlloc) "Prints free pages in a PageAllocator"
	proc explorePageAlloc {coid root {type PageAllocatorDefault}} {
		set type PageAllocatorDefault
		set rep [getReps $root]
		set avail [scalarResp [getField available $rep $type]]
		set freeList [scalarResp [getField anchor $rep $type]]
		puts "Available $avail"

		printTree $freeList
	}

## Process Support : needs init to be called first (to initialize processFD)
	set helpDB(processDetails) "Prints name, PID, isKern? and userMode for a given Process"
	proc processDetails {coid root type} {
		global processFD
		set response [getInstance $root $type]
		puts $processFD "$root [getReps $root] [strValue [RHS $response "name = "]] [numValue [RHS $response "processID = "]]"
		flush $processFD
		#puts -nonewline [strValue [RHS $response "name = "]]
		#puts -nonewline " (K [numValue [RHS $response "isKern = "]],"
		#puts -nonewline "U [numValue [RHS $response "userMode = "]])  "
		#puts "PID [numValue [RHS $response "processID = "]]"
		return {}
	}

	set helpDB(processListRegions) "Prints details of Regions associated with a Process"
	proc processListRegions {coid root type} {
		set response [sendToGDB "p (('$type' *)$root)->rlst.regions"]
		set obj [split $response "= \{\}"]
		set rtype [lindex $obj 5]
		set rptr [lindex $obj 7]
		set count 0
		while {[string compare $rptr "0x0"] && [string compare $rptr ""]} {
			set response [getInstance $rptr $rtype]
			puts "    type [alphaNum [RHS $response "type = "]], vaddr [numValue [RHS $response "vaddr = "]], size [numValue [RHS $response "size = "]], reg [numValue [RHS $response "reg = "]]"
			set rptr [numValue [RHS $response "next = "]]
			set count [expr $count+1]
		}
		puts "*** $count ***"
		return {}
	}

	proc processAll {coid root type} {
		processDetails $coid $root $type
		processListRegions $coid $root $type
		puts "-----------------------------------------------"
		return {}
	}

 ## Region proc's

	set helpDB(exploreRegion) "Prints details of Region specified by root. Fills r2p, r2fcm\
	and returns list of FCMs"
	proc exploreRegion {coid root {type RegionDefault}} {
		global r2p r2fcm rlist
		
		set response [getInstance [getReps $root] $type]
		set procref [numValue [RHS $response "proc = "]]
		set procresponse [sendToGDB "p (**(ProcessRef)$procref)->myRoot"]
		puts -nonewline "Proc [numValue [RHS $procresponse "\[\\\)\]\[\\\ \]"]],"
		set fcmref [numValue [RHS $response "fcm = "]]
		set fcmresponse [sendToGDB "p (**(FCMRef)$fcmref)->myRoot"]
		puts -nonewline " FCM [numValue [RHS $fcmresponse "\[\\\)\]\[\\\ \]"]],"
		puts -nonewline " Vaddr [numValue [RHS $response "regionVaddr = "]]"
		puts -nonewline " Offset [numValue [RHS $response "fileOffset = "]]"
		puts -nonewline " align [numValue [RHS $response "alignment = "]]"
		puts -nonewline " pSize [numValue [RHS $response "pageSize = "]]"
		puts -nonewline " aSize [numValue [RHS $response "attachSize = "]]"
		puts " RegSize [numValue [RHS $response "regionSize = "]]"

		set r2p($root) [numValue [RHS $procresponse "\[\\\)\]\[\\\ \]"]]
		set r2fcm($root) [numValue [RHS $fcmresponse "\[\\\)\]\[\\\ \]"]]
		lappend rlist $root
		return $rlist
	}

	proc exploreFCMStartup {c r {t FCMStartup}} {
		global fcmstartup
		
		set rep [getReps $r]
		lappend fcmstartup "$r [scalarResp [getField imageOffset $rep $t]] [scalarResp [getField imageSize $rep $t]]"
		return {}
	}

	set helpDB(memReset) "Resets global variables used to store fcm2pm, fcm2fr, etc."
	proc memReset {} {
		global r2p r2fcm rlist fcm2fr fcm2pm fcmlist fcmstartup pa pmroot
		foreach var {r2p r2fcm rlist fcm2fr fcm2pm fcmlist fcmstartup pa pmroot} {
			if [info exists $var] {
				unset $var
			}
		}
	}

	proc init {} {
		global FP processFD
		set FP [open "map" {RDWR CREAT}]
		set processFD [open "process_details" {RDWR CREAT}]
	}

	proc cleanup {} {
		global FP processFD
		close $FP
		close $processFD
	}
	
	proc writeToDisk {} {
		global r2p r2fcm rlist fcm2fr fcm2pm fcmlist fcmstartup pa pmroot

		set fp [open "rlist" {RDWR CREAT}]
		foreach item $rlist {
			puts $fp $item
		}
		close $fp

		set fp [open "freepages" {RDWR CREAT}]
		foreach item $pa {
			puts $fp $item
		}
		close $fp

		set fp [open "fcmlist" {RDWR CREAT}]
		foreach item $fcmlist {
			puts $fp $item
		}
		close $fp
	
		set fp [open r2fcm {RDWR CREAT}]
		foreach item $rlist {
			puts $fp $r2fcm($item)
		}
		close $fp
		set fp [open r2p {RDWR CREAT}]
		foreach item $rlist {
			puts $fp $r2p($item)
		}
		close $fp
	
		foreach var {r2p r2fcm} {
			set fp [open $var {RDWR CREAT}]
			foreach item $rlist {
				eval "puts $fp $${var}($item)"
			}
			close $fp
		}

		foreach var {fcm2fr fcm2pm} {
			set fp [open $var {RDWR CREAT}]
			foreach item $fcmlist {
				eval "puts $fp $${var}($item)"
			}
			close $fp
		}

		set fp [open "PMRoot" {RDWR CREAT}]
		foreach item $pmroot {
			puts $fp $item
		}
		close $fp

		set fp [open "FCMStartup" {RDWR CREAT}]
		foreach item $fcmstartup {
			puts $fp $item
		}
		close $fp
}

	proc dumpMem {} {
		memReset
		puts "Initialize"
		init
		puts "Processes"
		processObjs ProcessRep processDetails
		puts "PageAllocatorKernPinned"
		processObjs PageAllocatorKernPinned explorePageAlloc
		puts "PMRoot"
		processObjs PMRoot explorePMRoot
		puts "FCMStartup"
		processObjs FCMStartup exploreFCMStartup
		puts "FCMs"
		return {}
		processObjs FCM exploreFCM
		puts "Regions"
		processObjs RegionDefault exploreRegion
		cleanup
		writeToDisk
	}

}

MemSupport
