# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: gui.tcl,v 1.3 2005/08/19 23:11:20 bseshas Exp $

proc GraphicsSupport {} {
    proc setupGraphics {} {
        global canvasWidth canvasHeight objImgFile objImgHeight objImgWidth \
            objLabelFont gdbResCnt instanceCount instanceStr filterRegex
        
        set canvasWidth 800
        set canvasHeight 500
        set objImgFile "/home/bala/k42/kitchsrc/tools/misc/kore-dev/lib/images/co.gif"
        set objImgHeight 0
        set objImgWidth  0
        set objLabelFont "*-times-medium-r-normal--*-90-*-*-*-*-*-*"
        set gdbResCnt 0        
        set instanceStr ""
        set instanceCount 0
        set filterRegex ".*"
    }

    proc createViewer {name} { 
        global canvasWidth canvasHeight objImgFile objImgHeight objImgWidth \
            objLabelFont gdbResCnt instanceCount instanceStr filterRegex 

        toplevel $name
        frame $name.re
        label $name.re.regexlabel -text "Filter:"
        entry $name.re.regex -width 40 -relief sunken -bd 2 \
            -textvariable filterRegex
        pack $name.re.regexlabel $name.re.regex -side left -padx 1m \
            -pady 2m -fill x -expand 1
        
        frame $name.ob    
        canvas $name.ob.objcan -height $canvasHeight -width $canvasWidth \
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
        
        pack $name.re -side top -fill x -expand 1
        pack $name.ob -fill both -expand 1
        pack $name.info -fill x -expand 1
        
        .mainViewer.ob.objcan bind Object <Any-Enter> {
            set data [.mainViewer.ob.objcan gettags current]
            set instanceStr \
                "ref=[lindex $data 0] root=[lindex $data 1] type=[lindex $data 2]"
        }
        
        .mainViewer.ob.objcan bind Object <Any-Leave> {
            set instanceStr ""
        }
                
        .mainViewer.ob.objcan bind Object <Button-1> { 
            set data [.mainViewer.ob.objcan gettags current]
            set type [lindex $data 2]
            displayTypeDef $type
        }
        
        proc config {w h} {
            global canvasWidth canvasWidth
            set canvasWidth $w
            set canvasHeight $h
        }
        
        bind .mainViewer.ob.objcan <Configure> "config %w %h"
        
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
           # closeObjectStream
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
            set instanceCount [displayObjs .mainViewer.ob.objcan $filterRegex mco] 
            redisplay
        }
    }

    proc startGraphicsInterface {} {
        global filterRegex
		package require Tk
        createViewer .mainViewer
        set instanceCount [displayObjs .mainViewer.ob.objcan $filterRegex mco]
        redisplay
    }

    setupGraphics
}

GraphicsSupport
