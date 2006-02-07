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
#  $Id: utils.tcl,v 1.2 2005/03/14 20:11:34 butrico Exp $
# ############################################################################

proc display_gprs { sim } {
	set r0 [$sim cpu 0 display gpr 0]
	set r1 [$sim cpu 0 display gpr 1]
	set r2 [$sim cpu 0 display gpr 2]
	set r3 [$sim cpu 0 display gpr 3]
	set r4 [$sim cpu 0 display gpr 4]
	set r5 [$sim cpu 0 display gpr 5]
	set r6 [$sim cpu 0 display gpr 6]
	set r7 [$sim cpu 0 display gpr 7]
	set r8 [$sim cpu 0 display gpr 8]
	set r9 [$sim cpu 0 display gpr 9]
	set r10 [$sim cpu 0 display gpr 10]
	set r11 [$sim cpu 0 display gpr 11]
	set r12 [$sim cpu 0 display gpr 12]
	set r13 [$sim cpu 0 display gpr 13]
	set r14 [$sim cpu 0 display gpr 14]
	set r15 [$sim cpu 0 display gpr 15]
	set r16 [$sim cpu 0 display gpr 16]
	set r17 [$sim cpu 0 display gpr 17]
	set r18 [$sim cpu 0 display gpr 18]
	set r19 [$sim cpu 0 display gpr 19]
	set r20 [$sim cpu 0 display gpr 20]
	set r21 [$sim cpu 0 display gpr 21]
	set r22 [$sim cpu 0 display gpr 22]
	set r23 [$sim cpu 0 display gpr 23]
	set r24 [$sim cpu 0 display gpr 24]
	set r25 [$sim cpu 0 display gpr 25]
	set r26 [$sim cpu 0 display gpr 26]
	set r27 [$sim cpu 0 display gpr 27]
	set r28 [$sim cpu 0 display gpr 28]
	set r29 [$sim cpu 0 display gpr 29]
	set r30 [$sim cpu 0 display gpr 30]
	set r31 [$sim cpu 0 display gpr 31]
	puts "   R00=$r0  R01=$r1  R02=$r2  R03=$r3"
	puts "   R04=$r4  R05=$r5  R06=$r6  R07=$r7"
	puts "   R08=$r8  R09=$r9  R10=$r10  R11=$r11"
	puts "   R12=$r12  R13=$r13  R14=$r14  R15=$r15"
	puts "   R16=$r16  R17=$r17  R18=$r18  R19=$r19"
	puts "   R20=$r20  R21=$r21  R22=$r22  R23=$r23"
	puts "   R24=$r24  R25=$r25  R26=$r26  R27=$r27"
	puts "   R28=$r28  R29=$r29  R30=$r30  R31=$r31"
}
proc disasm_mem { sim addr insts } {
	set index 0
	while { $index < $insts } {
		set eaddr [expr $addr + ($index * 4)]
		set raddr [$sim util itranslate $eaddr]
		set inst [$sim memory display $raddr 4]
		set disasm [$sim util ppc-disasm $inst $eaddr]
		set hex_eaddr [format "0x%08X" $eaddr]
		set hex_raddr [format "0x%08X" $raddr]
		puts "EADDR:$hex_eaddr RADDR:$hex_raddr Enc:$inst : $disasm"

		incr index
	}
}
proc display_state { sim } {
	set pc [$sim cpu 0 display spr pc]
	set pc_raddr [$sim util itranslate $pc]
	set lr [$sim cpu 0 display spr lr]
	set lr [$sim cpu 0 display spr lr]
	set msr [$sim cpu 0 display spr msr]
	set inst [$sim memory display $pc_raddr 4]
	set disasm [$sim util ppc-disasm $inst $pc]
	puts "PC:$pc (Real:$pc_raddr) Enc:$inst : $disasm"
	puts "    LR=$lr  MSR=$msr"
	display_gprs $sim
}

proc xgo { sim } {
    while { 1 } {
	$sim cycle 1
	set pc [$sim cpu 0 display spr pc]
	if { $pc == 0x300 } {
	    break
	}
	if { $pc > 0x0200000 } {
	    set pc_raddr [$sim util itranslate $pc]
	    set inst [$sim memory display $pc_raddr 4]
	    set disasm [$sim util ppc-disasm $inst $pc]
	    puts "PC:$pc_raddr Enc:$inst : $disasm"
	}
    }
}
proc tgo { sim count } {
    while { $count > 0 } {
	$sim cycle 1
	set pc [$sim cpu 0 display spr pc]
	set pc_raddr [$sim util itranslate $pc]
	set inst [$sim memory display $pc_raddr 4]
	set disasm [$sim util ppc-disasm $inst $pc]
	puts "PC:$pc_raddr Enc:$inst : $disasm"

	incr count -1
    }
}

# Go until the next call or return
proc cgo { sim } {
	set ret  [expr 0x43800020 + 0]
	set hret [format "%08X" $ret]
	set call [expr 0x48000001 + 0]
	set hcall [format "%08X" $call]
	while { 1 } {
		$sim cycle 1
		#display_state mysim
		set pc [$sim cpu 0 display spr pc]
		set pc_raddr [$sim util itranslate $pc]
		set rinst [expr [$sim memory display $pc_raddr 4] + 0]
		set minst [expr $rinst & 0xFC000001]
		set hinst [format "%08X" $rinst]
		#puts "inst is $hinst ($rinst)"
		if { $rinst == $ret } {
			puts "Return reached"
			display_state mysim
			return
		} else {
			#puts "$hinst != $hret"
		}
		if { $minst == $call } {
			puts "Call reached"
			set foo [exec grep [format "%08x:" $pc] vmlinux.dis]
			puts "Call to $foo"
			display_state mysim
			return
		} else {
			#puts "$hinst != $hcall"
		}
	}
}
proc vcycle { sim } {
	display_state mysim
	$sim cycle 1
}
proc vgo { sim } {
	while { 1 } {
		vcycle $sim
	}
}
proc vstep { sim count } {
	while { $count > 0 } {
		vcycle $sim
		incr count -1
	}
}
proc wgo { sim addr } {
	while { 1 } {
		set pc [$sim cpu 0 display spr pc]
		if { $pc == $addr } {
			puts "Found address at $addr\n"
		}
		$sim cycle 1
	}
}

proc msrgo { sim } {
    # this does not work
    set msr [$sim cpu 0 display spr msr]
    while { [expr $msr & 0x001000] != 0 } {
	$sim cycle 1
	set msr [$sim cpu 0 display spr msr]
    }
}

proc dump_pagetbl { location length } {
    for { set i $location } { $i < [expr $location + $length] } { incr i 16 } {
	set val1 [mysim memory display $i 8]
	set val2 [mysim memory display [expr $i + 8] 8]]
        if {!(($val1 == 0 ) || ( $val2  == 0 )) } {
	    puts [format "%08X: %s %s" $i [mysim memory display $i 8] [mysim memory display [expr $i + 8] 8]]
	}
    }
}

proc ext_int { sim args } {
    set pc [mysim cpu 0 display spr pc]
    set msr [mysim cpu 0 display spr msr]
    set pc_laddr [mysim util itranslate $pc]
    set hsrr0 [$sim cpu 0 display spr hsrr0]
    set hsrr1 [$sim cpu 0 display spr hsrr1]
    set srr0 [$sim cpu 0 display spr srr0]
    set srr1 [$sim cpu 0 display spr srr1]
    set pend [$sim memory display 0xe030d400 8]
    puts "exception $pc_laddr ($msr): $pend"
    puts "  hsrr0: $hsrr0, hsrr1: $hsrr1"
    puts "  srr0:  $srr0, srr1:  $srr1"
}

proc dpage_fault { sim args } {
	set pc [$sim cpu 0 display spr pc]
	set ipc [$sim cpu 0 display spr srr0]
	set dsisr [$sim cpu 0 display spr dsisr]
	set srr1 [$sim cpu 0 display spr srr1]
	set msr [$sim cpu 0 display spr msr]
	set ea [$sim cpu 0 display spr dar]
#	puts "$pc: data page fault for PC:$ipc EA:$ea DSISR:$dsisr ASR:$asr"
    puts "exception $pc"
    puts "srr0: $ipc, srr1: $srr1"
    puts "dar: $ea, dsisr: $dsisr"

#	simstop
}
#mysim trigger set pc 0x300 "dpage_fault mysim"

proc ipage_fault { sim args } {
	set pc [$sim cpu 0 display spr pc]
	set ipc [$sim cpu 0 display spr srr0]
	puts "$pc: inst page fault for PC:$ipc"
	simstop
}

#
# behave like gdb
#
proc p { reg } {
    switch -regexp $reg {
	^r[0-9]+$ {
	    regexp "r(\[0-9\]*)" $reg dummy num
	    set val [mysim cpu 0 display gpr $num]
	    puts "$val"
	}
	^f[0-9]+$ {
	    regexp "f(\[0-9\]*)" $reg dummy num
	    set val [mysim cpu 0 display fpr $num]
	    puts "$val"
	}
	^v[0-9]+$ {
	    regexp "v(\[0-9\]*)" $reg dummy num
	    set val [mysim cpu 0 display vmxr $num]
	    puts "$val"
	}
	default {
	    set val [mysim cpu 0 display spr $reg]
	    puts "$val"
	}
    }
}

#
# behave like gdb
#
proc sr { reg val } {
    switch -regexp $reg {
	^[0-9]+$ {
	    mysim cpu 0 set gpr $reg $val
	}
	default {
	    mysim cpu 0 set spr $reg $val
	}
    }
    pr $reg
}

set sysmap "/home/jimix/work/hypervisor/build/System.map-rmor"

proc symaddr { sym } {
    # use head to stop searching after first match
    set addr [sed -ne 's/\(\[0-9a-f\]*\) . \.$sym/\1/p' < $sysmap | head -1]
    puts "0x$addr"
}
proc addrsym { addr } {
    # tolower
    set addr [echo $addr | tr 'A-Z' 'a-z']
    # strip the 0x if there
    regsub 0x $foo "" foo
    # use head to stop searching after first match
    set sym [sed -ne 's/0*$addr . \(.*\)$/\1/p' < $sysmap | head -1]
    puts "0x$addr"
}

proc b { addr } {
    mysim trigger set pc $addr "just_stop mysim"
    set at [i $addr]
    puts "breakpoint set at $at"
}

proc c { } {
    mysim go
}

proc i { pc } {
    set pc_laddr [mysim util itranslate $pc]
    set inst [mysim memory display $pc_laddr 4]
    set disasm [mysim util ppc-disasm $inst $pc]
    puts "PC:$pc ($pc_laddr) Enc:$inst : $disasm"
}

proc ipc { } {    
    set pc [mysim cpu 0 display spr pc]
    i $pc
}

proc s { } {
    mysim cycle 1
    ipc
}

proc z { count } {
    while { $count > 0 } {
	s
	incr count -1
    }
}

proc sample_pc { sample count } {
    while { $count > 0 } {
	mysim cycle $sample
	ipc
	incr count -1
    }
}

proc reset {} {
    mysim load elf $my_image
}

proc e2p { ea } {
    set pa [ mysim util dtranslate $ea ]
    puts "$pa"
}

proc x {  pa { size 8 } } {
    set val [ mysim memory display $pa $size ]
    puts "$pa : $val"
}

proc hexdump { location count }    {
    set addr  [and64 $location 0xfffffffffffffff0 ]
    set top [add64 $addr +[expr $count * 15]]
    for { set i $addr } { $i < $top } { incr i 16 } {
	set val [add64 $i +[expr 4 * 0]]
	set val0 [format "%08x" [mysim memory display $val 4]]
	set val [add64 $i +[expr 4 * 1]]
	set val1 [format "%08x" [mysim memory display $val 4]]
	set val [add64 $i +[expr 4 * 2]]
	set val2 [format "%08x" [mysim memory display $val 4]]
	set val [add64 $i +[expr 4 * 3]]
	set val3 [format "%08x" [mysim memory display $val 4]]
	set ascii "(none)"
	set loc [format "0x%016x" $i]
	puts "$loc: $val0 $val1 $val2 $val3 $ascii"
    }
}

proc slbv {arg} {
   puts "[mysim cpu 0 display slb valid]"
}

set hdec_cycles 0
proc hdec {arg} {
    global hdec_cycles
    set pc [mysim cpu 0 display spr hsrr0]
    set msr [mysim cpu 0 display spr hsrr1]
    set cycles [mysim display cycles]
    set delta [expr $cycles - $hdec_cycles]
    set hdec_cycles $cycles
    puts "HDECR: msr=$msr PC=$pc cycles=$cycles del=$delta"
    puts "[mysim cpu 0 display slb valid]"
}

proc hcall {arg} {
    set pc [mysim cpu 0 display spr hsrr0]
    set pc_raddr [mysim util itranslate $pc]
    set r3 [mysim cpu 0 display gpr 3]
    
    puts "HCALL: r3=$r3 PC=$pc_raddr"
    puts "[mysim cpu 0 display slb valid]"
}

proc just_stop {sim args} {simstop}

proc st { count } {
    set sp [mysim cpu 0 display gpr 1]
    puts "SP: $sp"
    ipc
    set lr [mysim cpu 0 display spr lr]
    i $lr
    while { $count > 0 } {
	set sp [mysim util itranslate $sp]
	set lr [mysim memory display [add64 $sp +16] 8]
	i $lr
	set sp [mysim memory display $sp 8]
	
	incr count -1
    }
}

proc watch { exp } {
    while { $exp } {
	mysim cycle 1
    }
    puts "condition occured $exp"
}

#
# force gdb to attach
#
proc gdb { {t 0} } {
# until fast mode supports breakpoints, turn it off here
    mysim modify fast off
    mysim debugger wait $t
}

proc prog_fault { sim args } {
    set fpc [$sim cpu 0 display spr srr0]
    set srr1 [$sim cpu 0 display spr srr1]
    set msr [$sim cpu 0 display spr msr]
    set sprg0 [$sim cpu 0 display spr sprg0]
    puts "program interrupt: fault pc: $fpc"
    puts "srr0: $fpc, srr1: $srr1 msr: $msr"
    puts "sprg0: $sprg0"

    simstop
}
#mysim trigger set pc 0x700 "prog_fault mysim"

proc simulation_stop { sim args } {
    display_state $sim
    simstop
}
#mysim trigger set pc 0xC0000000022A806C "simulation_stop mysim"
