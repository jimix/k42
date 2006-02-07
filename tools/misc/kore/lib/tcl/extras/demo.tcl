# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: demo.tcl,v 1.1 2005/08/19 23:15:25 bseshas Exp $

proc DemoSupport {} {

	proc resetDemoVars {} {
		global kount histoHash
		set kount 0
		unset histoHash
	}

	proc countObjs {c r t} {
		global kount
		incr kount
	}

	proc histogram {c r t} {
		global histoHash
		if {[info exists histoHash($t)]} {
			incr histoHash($t)
			return ""
		} else {
			set histoHash($t) 1
			return $t
		}
	}

	proc printField {c r t} {
	}

	proc printReps { } {
	}

}

DemoSupport
