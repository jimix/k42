##############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: start.s,v 1.3 2000/05/11 11:29:26 rosnbrg Exp $
##############################################################################

	.file	"start.s"
	.machine "ppc"

	.csect	init[PR]
__start:
	.globl	__start
	bl	here			# get the address of "here"
here:	mflr	r31
	lwz	r2,ltoc-here(r31)	# Get the address of the TOC
	bl	.main[PR]
	.extern	.main[PR]

ltoc:	.long	TOC_ORIGIN		# TOC[t0]
	.globl	ltoc

	.toc				# ensure that TOC section exists
