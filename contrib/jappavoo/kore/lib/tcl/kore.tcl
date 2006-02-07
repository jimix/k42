# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: kore.tcl,v 1.2 2005/06/17 21:33:12 jappavoo Exp $

proc KoreSupport {} {
    global env

    if [info exists env(KORE_LIBDIR)] {
        global korePath
        set korePath "$env(KORE_LIBDIR)/tcl"
    }

    if ![info exists korePath] {
        global korePath
        set korePath "."
    }
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
}

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
             {set fd [open "|powerpc64-linux-nm -a -C $symFile" r]} ]} {
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

proc oisSupport {} {
    proc setupOISSupport {} {
        global oisHost oisPort oisFD
        set oisHost kxs6
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
                #            puts "line: $line"    
                lappend response $line
            }
        }
        close $oisFD
        puts "Error: getOISResponse failed"
        return {}
    }
    
    setupOISSupport
}


proc BasicObjSupport {} {

    proc setupBasicObjSupport {} {
        global host port
        set host kxs6
        set port 3624
        return 1
    }

    proc openObjectStream {} {
 #       if { [openOISConnection] == -1 } {
 #           return 0
 #       }
        sendToOIS "getObjectList" 
        return 1
    }

    proc getObject {} {
        set obj {}
        if {[set line [getOISResponseLine]] != ""} {
            set obj [split $line]
            set obj [lreplace $obj 2 2 [getType [lindex $obj 2]]]
        }
        return $obj
    }
    
    proc closeObjectStream {} {
        closeOISConnection
    }

    proc doPeriodic {func} {
        global periodicFuncs
    }

    proc updateObjs {filter} {
        global coid_array root_array type_array old_coid old_root old_type count old_count
	array unset old_coid
	array unset old_root
	array unset old_type
	set old_count 0
	while {1} {  # Infinite loop
            if [openObjectStream] {
	        set count 0
                while { [set obj [getObject]] != {} } {
                    if { [regexp $filter $obj] } {
                        set coid [lindex $obj 0]
                        set root [lindex $obj 1]
                        set type [lindex $obj 2]
	                set coid_array($count) $coid
	     	        set root_array($count) $root
		        set type_array($count) $type
			set count [expr {$count + 1}]
                    }
                }
	        compareAndPrint old_coid old_root old_type $old_count coid_array root_array type_array $count
		puts "-------------------"
	        set old_count $count
    	        array set old_coid [array get coid_array]
	        array set old_root [array get root_array]
		array set old_type [array get type_array]
            }
	    after 1000
	}
    }

    proc compareAndPrint {old_coid old_root old_type old_count coid_array root_array type_array count} {
        upvar $old_coid o_coid
	upvar $old_root o_root
	upvar $old_type o_type
	upvar $coid_array n_coid
	upvar $root_array n_root
	upvar $type_array n_type

	set o 0
	set n 0
	while {$o < $old_count && $n < $count} {
	    if {$o_coid($o) == $n_coid($n)} {
	        set o [expr {$o + 1}]
	        set n [expr {$n + 1}]
	    } elseif {$o_coid($o) < $n_coid($n)} {
	        puts "--- $o_coid($o) $o_root($o) $o_type($o)"
	        set o [expr {$o + 1}]
	    } elseif {$n_coid($n) < $o_coid($o)} {
	        puts "+++ $n_coid($n) $n_root($n) $n_type($n)"
	        set n [expr {$n + 1}]
	    }
	}
	
	if {$o == $old_count} {
	    while {$n < $count} {
	        puts "+++ $n_coid($n) $n_root($n) $n_type($n)"
	        set n [expr {$n + 1}]
	    }
	}
	if {$n == $count} {
	    while {$o < $old_count} {
	        puts "--- $o_coid($o) $o_root($o) $o_type($o)"
	        set o [expr {$o + 1}]
	    }
	}
    }

    proc processObjs {filter func} {
    }

    proc printObjs {filter} {
    	if [openObjectStream] {
            while { [set obj [getObject]] != {} } {
                if { [regexp $filter $obj] } {
                    set coid [lindex $obj 0]
                    set root [lindex $obj 1]
                    set type [lindex $obj 2]
                    puts "coid=$coid root=$root type=$type"
                }
            }
	}
    }

    proc onCreate {type func} {
        puts "onCreate $type $func"
    }

    proc onDestroy {type func} {
        puts "onDestroy $type $func"
    }


    proc hotSwap {fac root} {
        sendToOIS "hotSwapInstance,$fac,$root"
    } 

    proc takeOver {newFac oldFac} {
        sendToOIS "takeOverFromFac,$newFac,$oldFac"
    }

    setupBasicObjSupport
}

proc GDBSupport {} {
    proc setupGDB {} {
        global gdbFD gdbPath gdbImageFile

        set gdbImageFile "/u/bseshas/k42/powerpc/fullDeb/os/boot_image.dbg"
        set gdbPath "powerpc64-linux-gdb"

        set gdbFD [open "|$gdbPath -n -q -f 2>&1 | cat" r+]

        fconfigure $gdbFD -buffering line -translation binary
        #    fconfigure $gdbFD -buffering none
        initGDB
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
        loadGDBInitExecutable $gdbImageFile
    }

    proc gdbConnect {host} {
        sendToOIS "getInspectPort"
        set port [getOISResponse]
        puts "Connecting gdb to $host $port\n";
        gdbTargetHost $host $port
    }

    proc gdbTargetHost {host port} {
        sendToGDB "target remote $host:$port"
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
                #            puts "found gdb respone termination: $line"
                return $response
            } else {
                #            puts "line: $line"    
                lappend response $line
            }
        }
        puts "Error: getGDBResponse failed"
        return {}
    }

    proc sendToGDB { cmd } {
        global gdbFD
        #    puts "sending : $cmd"
        puts -nonewline $gdbFD "$cmd\n"
        return [getGDBResponse]
    }

    proc printGDBResponse {response} {
        puts [join $response "\n"]
    }
    
    proc getTypeDefinition { type } {
        puts "getTypeDefinition $type"
        return [sendToGDB "ptype struct $type"]
    }


    proc getInstance {ptr type} {
        return [sendToGDB "p *(struct $type  *)$ptr"]
    }

    proc printInstance {ptr type} {
        sendToGDB "set print pretty on"
        printGDBResponse [getInstance $ptr $type]
        sendToGDB "set print pretty off"
    }

    proc getField {field ptr type} {
        return [sendToGDB "p ((struct $type *)$ptr)->$field"]
    }


    setupGDB
}

proc COSupport {} {
    proc setupCOSupport {} {
    }

    proc getRoot {root} {
        return [getInstance $root COSMissHandlerBase]
    }

    proc getRep {rep} {
        return [getInstance $rep COSTransObject]
    }
    
    proc getCO {instance} {
        set root [getRoot $instance]
        
    }
    proc isa {obj} {
    }

}

proc GraphicsSupport {} {
    proc setupGraphics {} {
        global canvasWidth canvasHeight objImgFile objImgHeight objImgWidth \
            objLabelFont gdbResCnt instanceCount instanceStr filterRegex
        
        set canvasWidth 600
        set canvasHeight 500
        set objImgFile "~/k42-d05/kitchsrc/contrib/jappavoo/scripts/co.gif"
        set objImgHeigth 0
        set objImgWidth  0
        set objLabelFont "*-times-medium-r-normal--*-90-*-*-*-*-*-*"
        set gdbResCnt 0        
        set instanceStr ""
        set instanceCount 0
        set filterRegex ".*"
    }

    proc createViewer {name} { 
        toplevel $name
        frame $name
        label $name.regexlabel -text "Filter:"
        entry $name.regex -width 40 -relief sunken -bd 2 \
            -textvariable filterRegex
        pack $name.regexlabel $name.regex -side left -padx 1m \
            -pady 2m -in $name -fill x -expand 1
        
        frame $name.ob    
        canvas $name.ob.objcan -height $canvasHeight -width $canvasHeight \
            -background white -yscrollcommand [list $name.ob.yscroll set] \
            -borderwidth 0 -scrollregion {0 0 0 100000}
        scrollbar $name.ob.yscroll -orient vertical -command [list $name.ob.objcan yview]
        grid $name.ob.objcan $name.ob.yscroll -sticky news
        grid columnconfigure $name.ob 0 -weight 1
        
        frame $name.info
        label $name.info.inlbl -text "Count: "
        label $name.info.incnt -textvariable instanceCount
        label $name.info.indata -textvariable instanceStr
        pack $name.info.inlbl $name.info.incnt $name.info.indata -side left \
            -padx 1m -pady 2m -in $name.info -fill x -expand 1
        
        pack $name -side top -fill x -expand 1
        pack $name.ob -fill both -expand 1
        pack $name.info -fill x -expand 1
        
        $name.ob.objcan bind Object <Any-Enter> {
            set data [$name.ob.objcan gettags current]
            set instanceStr \
                "ref=[lindex $data 0] root=[lindex $data 1] type=[lindex $data 2]"
        }
        
        $name.ob.objcan bind Object <Any-Leave> {
            set instanceStr ""
        }
                
        $name.ob.objcan bind Object <Button-1> { 
            set data [$name.ob.objcan gettags current]
            set type [lindex $data 2]
            displayTypeDef $type
        }
        
        proc config {w h} {
            global canvasWidth canvasWidth
            set canvasWidth $w
            set canvasHeight $h
        }
        
        bind $name.ob.objcan <Configure> "config %w %h"
        
        image create photo mco -file $objImgFile
        set objImgHeight [image height mco]
        set objImgWidth [image width mco]
        return 1
    }
            
    proc mkDisplayObj {c id root type im iw x y lx ly} {
        global objLabelFont
        
        set obj [$c create image $x $y -image $im \
                     -tags [list $id $root $type Object]]
        set label [$c create text $lx $ly  -anchor n -text "$id\n$type" \
                       -width $iw -font $objLabelFont \
                       -tags [list $id ObjectLabel]]
        return $obj
    }

    proc displayObjs {c filter im} {
        global canvasWidth objImgWidth objImgHeight
        global host port
        set xinc [expr $objImgWidth / 2]
        set yinc [expr $objImgHeight / 2]
        set x $xinc
        set y $yinc
        set i 0

        $c delete Object ObjectLabel
        
        if [openObjectStream] {        
            while { [set obj [getObject]] != {} } {
                if { [regexp $filter $obj] } {
                    set coid [lindex $obj 0]
                    set root [lindex $obj 1]
                    set type [lindex $obj 2]
                    set i [expr $i + 1]
                    set yl [expr $y + $objImgHeight/2 + 1]
                    set obj [mkDisplayObj $c $coid $root $type\
                                 mco $objImgWidth $x $y $x $yl]
                    set x [expr $x + $objImgWidth + 4]
                    if { $x >= $canvasWidth } {
                        set x $xinc
                        set y [expr $y + $objImgHeight + $objImgHeight/2]
                    }
                }
            }
            closeObjectStream
        }
        return $i
    }

    proc displayGDBResponse {response} {
        global gdbResCnt
        #    puts $response
        set winName ".gdbRes$gdbResCnt"
        set gdbResCnt [expr $gdbResCnt + 1]
        toplevel $winName 
        message $winName.msg -text [join $response "\n"]
        button $winName.but -text close -command [list destroy $winName ]
        pack $winName.msg $winName.but -side top -in $winName    
    }

    proc displayTypeDef {type} {
        #    global instanceStr
        #    set instanceStr "getting type definition for $type"
        displayGDBResponse [getTypeDefinition $type]
    }

    proc redisplay {} {
        global filterRegex
        after 1000 { 
            set instanceCount [displayObjs $name.ob.objcan $filterRegex mco] 
            redisplay
        }
    }

    proc startGraphicsInterface {} {
        global filterRegex
        createViewer mainViewer
        set instanceCount [displayObjs mainViewer.ob.objcan $filterRegex mco]
        redisplay
    }

    setupGraphics
} 

KoreSupport
#SymbolSupport
#GDBSupport
#oisSupport
#COSupport
#BasicObjSupport
#GraphicsSupport

#loadSymTbl /u/bseshas/k42/powerpc/fullDeb/os/boot_image.dbg
puts "****:$tcl_rcFileName: Kore Init file sourced ...."
#openOISConnection

#if { 0 && $tcl_interactive == 0 } {
#    startGraphicsInterface
#} else {
#    set tcl_prompt1 {puts -nonewline  "kore> "}
#    set filterRegex ".*"
#    set sometext "Some text"
#    regexp $filterRegex $sometext
#    printObjs $filterRegex
#}
