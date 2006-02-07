/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: idt.C,v 1.17 2003/06/04 14:17:50 rosnbrg Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "exception/ExceptionLocal.H"
#include "sys/syscalls.H"
#include <misc/hardware.H>

extern code int0, int1;
extern "C" void early_printk(const char *fmt, ...);

void
InitIdt()
{
    codeAddress target;
    uval step;
    SysGate *idt;

    target = &int0;
    step = &int1 - &int0;

    /* define the default values for IDT entries */
    /* by default, all traps and interrupts go to "int0" */
    /* a select few (which are expected like page faults) are reset below */

    for(idt = Idt; idt < IdtEnd; idt++) {
	idt->type = SysGate::intrg;
	idt->setoffset(target);
	idt->segment = KERNEL_CS;
	idt->S=SysGate::system;
	idt->dpl = 0;
	idt->ist = 1 /* set to use Interrupt Stack number 1 (IST1) */;
	idt->P = SysGate::Present;

	target += step;
    }
}


void
initExceptionHandlers(VPNum vp)
{
  if (vp==0) {
    // these ints are available to problem programs
    
    /* debugger trap, breakpoint */
    Idt[3].dpl = 3;			// debugger int
    
    
    /* page faults */
    Idt[14].setoffset(&ExceptionLocal_PgfltException);
    /* page faults are special -- stay on kernel stack in kernel mode
       but switch to alternate stack (RSP0) in user mode. */
    Idt[14].ist = 0;
    
    
    
    /* the 8254 (PIT) interrupt vector has been relocated to
     * 32 (0x20), we use here a dummy handler which just eoi and
     * iret until the system is suffciently initialized.
     */
    // no op timer interrupt handler
    Idt[0x20].setoffset(&ExceptionLocal_NullTimerInterrupt);
    Idt[0x20].ist = 0;
    
    
    /* The remainder are for the Protected Procedure Calls,
       and the small number of actual K42 system calls */
    
    Idt[SYSCALL_SET_ENTRY_POINT].
			    setoffset(&ExceptionLocal_SetEntryPointSyscall);
    Idt[SYSCALL_SET_ENTRY_POINT].dpl = 3;
    
    Idt[SYSCALL_PROC_YIELD].setoffset(&ExceptionLocal_ProcessYieldSyscall);
    Idt[SYSCALL_PROC_YIELD].dpl = 3;
    
    Idt[SYSCALL_PROC_HANDOFF].setoffset(&ExceptionLocal_ProcessHandoffSyscall);
    Idt[SYSCALL_PROC_HANDOFF].dpl = 3;
    
    Idt[SYSCALL_IPC_CALL].setoffset(&ExceptionLocal_IPCCallSyscall);
    Idt[SYSCALL_IPC_CALL].dpl = 3;
    
    Idt[SYSCALL_IPC_RTN].setoffset(&ExceptionLocal_IPCReturnSyscall);
    Idt[SYSCALL_IPC_RTN].dpl = 3;
    
    Idt[SYSCALL_PPC_PRIMITIVE].setoffset(&ExceptionLocal_PPCPrimitiveSyscall);
    Idt[SYSCALL_PPC_PRIMITIVE].dpl = 3;
    
    Idt[SYSCALL_IPC_ASYNC].setoffset(&ExceptionLocal_IPCAsyncSyscall);
    Idt[SYSCALL_IPC_ASYNC].dpl = 3;
    
    Idt[SYSCALL_TIMER_REQUEST].setoffset(&ExceptionLocal_TimerRequestSyscall);
    Idt[SYSCALL_TIMER_REQUEST].dpl = 3;
  }
  
  /* define the TSS for interrupts */
  exceptionLocal.x86Task.RSP0 = 0; // set to thread micro stack
  exceptionLocal.x86Task.RSP1 = 0;
  exceptionLocal.x86Task.RSP2 = 0;
  exceptionLocal.x86Task.R1 = 0;		// reserved ignored
  exceptionLocal.x86Task.IST1 = 0;
  exceptionLocal.x86Task.IST2 = 0;
  exceptionLocal.x86Task.IST3 = 0;
  exceptionLocal.x86Task.IST4 = 0;
  exceptionLocal.x86Task.IST5 = 0;
  exceptionLocal.x86Task.IST6 = 0;
  exceptionLocal.x86Task.IST7 = 0;
  exceptionLocal.x86Task.R2 = 0;		// reserved ignored
  exceptionLocal.x86Task.R3 = 0;		// reserved ignored
  exceptionLocal.x86Task.IOMAP = 0;
  
  SysDesc * tr;
  tr = (SysDesc*)&(exceptionLocal.Gdt[KERNEL_TR >> 3]);
  tr->type = SysDesc::tss64_a;			// the non BUSY TSS type
  tr->S = SysDesc::system;
  tr->setlimit(sizeof(X86Task));
  tr->setbase((uval) &exceptionLocal.x86Task);
  tr->dpl = 0;
  tr->P = SysDesc::Present;
  tr->G = 0;
  
  // insists on having ax, not eax or rax ... picky
  __asm__ __volatile__("ltr %%ax"::"a" (KERNEL_TR));	
}


/* derived from Linux
 */

void init_PIC(unsigned int i)
{
        /*
         * Put the board back into PIC mode (has an effect
         * only on certain older boards).  Note that APIC
         * interrupts, including IPIs, won't work beyond
         * this point!  The only exception are INIT IPIs.
         */
        outbc(0x22, 0x70);
        outbc(0x23, 0x00);


        outbc(0x21, 0xff);       /* mask all of 8259A-1 */
        outbc(0xA1,0xff);       /* mask all of 8259A-2 */

        /*
         * outbc_p - this has to work on a wide range of PC hardware.
         */
        outbc_p(0x20, 0x11);     /* ICW1: select 8259A-1 init */
        outbc_p(0x21, 0x20 + 0); /* ICW2: 8259A-1 IR0-7 mapped to 0x20-0x27 */
        outbc_p(0x21, 0x04);     /* 8259A-1 (the master) has a slave on IR2 */
        outbc_p(0x21, 0x01);     /* master expects normal EOI */

        outbc_p(0xA0, 0x11);     /* ICW1: select 8259A-2 init */
        outbc_p(0xA1, 0x20 + 8); /* ICW2: 8259A-2 IR0-7 mapped to 0x28-0x2f */
        outbc_p(0xA1, 0x02);     /* 8259A-2 is a slave on master's IR2 */
        outbc_p(0xA1, 0x01);     /* (slave's support for AEOI in flat mode
                                    is to be investigated) */

	for (; i ; i--)
		;		/* wait for 8259A to initialize */
        outbc(0x21, 0xff );  /* restore master IRQ mask */
        outbc(0xA1, 0xff );  /* restore slave IRQ mask */
}

#if 0

extern "C" void imr_enable(uval);
extern "C" void imr_disable(uval);
extern "C" void settimer(uval);
extern "C" uval gettimer(void);

#define TIMER_SET_VAL (10000)

extern uval  ioInterruptCounter;
#include __MINC(bios.H)

void
testTimerHandlers()
{
    uval targetCnt;
    uval loopCnt;
    uval wasEnabled;

    settimer(TIMER_SET_VAL);
    early_printk("testTimerHandlers start: ioInterruptCounter %ld.\n",
	    ioInterruptCounter);
//    imr_enable(0);     // enable the PIT timer IRQ-0
    wasEnabled = hardwareInterruptsEnabled();
//    tassertMsg(wasEnabled, "should be enabled here\n");
    targetCnt = ioInterruptCounter + 100;
    loopCnt = 0;
    while (ioInterruptCounter < targetCnt) {
	loopCnt++;
    }
    // WHY DISABLE?????
    // asm ("cli"); // disable external interrupts
//    imr_disable(0);     // disable the PIT timer IRQ-0
    early_printk("testTimerHandlers end: ioInterruptCount %ld, loopCnt %ld.\n",
	    ioInterruptCounter, loopCnt);
}

#endif /* #if 0 */

void
fixupExceptionHandlers(VPNum vp)
{
    disableHardwareInterrupts();

    // timer interrupt handler
    Idt[0x20].setoffset(&ExceptionLocal_TimerInterrupt);
    Idt[0x20].ist = 1;

    enableHardwareInterrupts();
}

extern "C" void
interrupt_eoi_timer(void)
{
    uval ocw;
    ocw = 0x60;
    Koutb(_HARDWARE_PIC1_OCW, ocw);
}
