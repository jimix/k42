/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *      
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    provides initialization and usage interface to x86 Timer and external
 *    interrupts.
 * **************************************************************************/

VVVVVVVVVVVVVVVVVVV from x86 VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV FIXME pdb 


/*****************************************************************************
  On x86 we expect the following interrupts to come in from the
  Master 8259 Programmable Interrupt Controller PIC on the various IRQ
  lines of the processor.
  
  IRQ0 : CDC-Timer
  IRQ1 : Keyboard
  IRQ2 : Slave 8259 PIC
  IRQ3 : Communication Port 1
  IRQ4 : Communication Port 2
  IRQ5 : free
  IRQ6 : free
  IRQ7 : free
  IRQ8 : RealTime Clock
  IRQ13: EIDE Harddrive
  IRQ14: EIDE Harddrive

  ***************************************************************************/

#include <arch/x86/asm.h>
#include <kinclude.H>
#include "init/kernel.H"
#include <sys/arch/x86/asmConstants.H>

#define PIC1_ICW1    0x20
#define PIC1_ICW2    0x21
#define PIC1_ICW3    0x21
#define PIC1_ICW4    0x21
#define PIC1_ISR_IRR 0x20
#define PIC1_IMR     0x21

#define PIC1_OCW1    0x21		
#define PIC1_OCW2    0x20		
#define PIC1_OCW3    0x20
	
#define PIC2_ICW1    0xA0
#define PIC2_ICW2    0xA1
#define PIC2_ICW3    0xA1
#define PIC2_ICW4    0xA1

#define PIC2_OCW1    0xA1
#define PIC2_OCW2    0xA0
#define PIC2_OCW3    0xA0
#define PIC2_ISR_IRR 0xA0
#define PIC2_IMR     0xA1

/*
 *   extern "C" void hi_disable(uval intrno)
 */

C_TEXT_ENTRY(hi_disable)
	movl	4(%esp),%ecx		// get intrno into ecx
	pushf				// save flags and interrupt mask
	cli				// disable all interrupts
	cmpl	$16,%ecx		// check for valid intr-#
	jae	1f
	inb	$PIC2_OCW1,%al		// load PIC_OCW1 into cx
	mov	%al,%ah
	inb	$PIC1_OCW1,%al

	bts	%eax,%ecx		// set the bit

	outb	%al,$PIC1_OCW1		// and write out the new mask
	movb	%ah,%al
	outb	%al,$PIC2_OCW1
1:	popf				// pop flags and interrupt mask
	ret
		
/*
 *   extern "C" void hi_enable(uval intrno)
 */
C_TEXT_ENTRY(hi_enable)
	movl	4(%esp),%ecx		// get intrno into ecx
	pushf				// save flags and interrupt mask
	cli				// disable all interrupts
	cmpl	$16,%ecx		// check for valid intr-#
	jae	1f			
	inb	$PIC2_OCW1,%al		// load PIC_OCW1 into cx
	mov	%al,%ah
	inb	$PIC1_OCW1,%al

	btr	%eax,%ecx		// reset the bit

	outb	%al,$PIC1_OCW1		// and write out the new mask
	movb	%ah,%al
	outb	%al,$PIC2_OCW1
1:	popf				// pop flags and interrupt mask
	ret

/*
 *   extern "C" void imr_disable(uval intrno)
 */
C_TEXT_ENTRY(imr_disable)
	movl	4(%esp),%ecx		// get intrno into ecx
	pushf				// save flags and interrupt mask
	cli				// disable all interrupts
	cmpl	$16,%ecx		// check for valid intr-#
	jae	1f			
	inb	$PIC2_IMR,%al		// load PIC_IMR into cx
	mov	%al,%ah
	inb	$PIC1_IMR,%al

	bts	%ecx,%eax		// set the bit

	outb	%al,$PIC1_IMR		// and write out the new mask
	movb	%ah,%al
	outb	%al,$PIC2_IMR
1:	popf				// pop flags and interrupt mask
	ret

		
/*
 *   extern "C" void imr_e1nable(uval intrno)
 */
C_TEXT_ENTRY(imr_enable)
	movl	4(%esp),%ecx		// get intrno into ecx
	pushf				// save flags and interrupt mask
	cli				// disable all interrupts
	cmpl	$16,%ecx		// check for valid intr-#
	jae	1f			
	inb	$PIC2_IMR,%al		// load PIC_IMR into cx
	mov	%al,%ah
	inb	$PIC1_IMR,%al

	btr	%ecx,%eax		// reset the bit

	outb	%al,$PIC1_IMR		// and write out the new mask
	movb	%ah,%al
	outb	%al,$PIC2_IMR
1:	popf				// pop flags and interrupt mask
	ret

/********************************************************************/
C_TEXT_ENTRY(get_isr_mask)
	// shoudl disable interrupts
	// don't have to write the OCW3 command out again and again
	movb	$0b00001011,%al
	outb	%al,$PIC1_OCW3
	subl	%eax,%eax
	inb	$PIC1_ISR_IRR,%al
	ret

/********************************************************************/
C_TEXT_ENTRY(get_irr_mask)
	// shoudl disable interrupts
	// don't have to write the OCW3 command out again and again
	movb	$0b00001010,%al
	outb	%al,$PIC1_OCW3
	subl	%eax,%eax
	inb	$PIC1_ISR_IRR,%al
	ret

/********************************************************************/
C_TEXT_ENTRY(get_imr_mask)
	// shoudl disable interrupts
	// don't have to write the OCW3 command out again and again
	subl	%eax,%eax
	inb	$PIC1_IMR,%al
	ret

#define PIT_COUNTER        0x40
#define PIT_CONTROL	   0x43
	
#define PIT_MODE0_CMD      0b00110000
#define PIT_MODE3_CMD      0b00110110
#define PIT_LATCH_CMD      0b00000000
	         	
	
C_TEXT_ENTRY(settimer)
	movb	$PIT_MODE3_CMD,%al      
	outb	%al,$PIT_CONTROL
	movl	4(%esp),%eax		// get intrno into ecx
	outb	%al,$PIT_COUNTER
	movb	%ah,%al
	outb	%al,$PIT_COUNTER
	ret

C_TEXT_ENTRY(gettimer)
	movb	$PIT_LATCH_CMD,%al
	outb	%al,$PIT_CONTROL
	inb	$PIT_COUNTER,%al
	movb	%al,%ah
	inb	$PIT_COUNTER,%al
	xchg    %ah,%al
	ret

	
#if 0  // this is Jochen's code
	
;----------------------------------------------------------------------------
;
;       lost interrupt catcher (IRQ 7 and ISR bit 7 = 0)
;
;----------------------------------------------------------------------------

init_interrupt_catcher:

mov   bl,irq7_intr
mov   bh,0 SHL 5
mov   eax,offset lost_interrupt_catcher
call  define_idt_gate

ret


icod  ends


lost_interrupt_catcher:                  ; in the moment hardware IRQ 7
                                       ; is disabled
iretd








;----------------------------------------------------------------------------
;
;       rtc timer
;
;----------------------------------------------------------------------------

assume ds:codseg


icode


init_rtc_timer:

mov   bl,irq8_intr
mov   bh,0 SHL 5
mov   eax,offset rtc_timer_int
call  define_idt_gate

DO
      inrtc rtc_reg_a
      bt    eax,7
      REPEATC
OD
and   al,0F0h
add   al,7                 ; set to 512 Hz
outrt rtc_reg_a

inrtc rtc_reg_b
or    al,01001000b
outrt rtc_reg_b

inrtc rtc_reg_c           ; reset timer intr line            

in    al,PIC2_imr
and   al,11111110b
out   PIC2_imr,al

ret


icod  ends












timer_counter        equ 40h
timer_control        equ 43h

counter0_mode0_16_cmd   equ 00110000b
counter0_mode2_16_cmd   equ 00110100b
counter0_mode3_16_cmd   equ 00110110b
counter0_latch_cmd      equ 00000000b


 die Makros sind leider nur zum messen geeignet. timer_start laedt den
 Timer mit FFFF, timer_stop liefert die die bisher vergangenen Ticks.


timer_start macro

mov al,counter0_mode0_16_cmd
out [timer_control],al
jmp $+2
jmp $+2
mov al,0FFh
out [timer_counter],al
jmp $+2
jmp $+2
out [timer_counter],al
endm



timer_stop macro

sub eax,eax
mov al,counter0_latch_cmd
out [timer_control],al
jmp $+2
jmp $+2
in  al,[timer_counter]
mov ah,al
jmp $+2
jmp $+2
in  al,[timer_counter]
xchg ah,al
neg  ax
endm



#endif
