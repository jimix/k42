/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Provides an interface to x86 APIC registers
 * **************************************************************************/

#include "kernIncs.H"
#include "APIC.H"

void
APIC::init()
{
    // FIXME:  We may want to first setup LVT1 and LVT2
    // Here.
    uval32 tmp;
    
    tmp = getTaskPriority();
    tmp &= ~APIC_TPR_PRIO;	/* Set Task Priority to 'accept all' */
    setTaskPriority(tmp);
    
    tmp = getSIV();
    tmp |= APIC_SVR_SWEN;		/* Enable APIC (bit==1) */
    tmp &= ~APIC_SVR_FOCUS;		/* Enable focus processor (bit==0) */
    
    tmp &= ~APIC_SVR_VEC_PROG;	/* clear (programmable) vector field */
    // FIXME: we don't understand this
    tmp |= (XSPURIOUSINT_OFFSET & APIC_SVR_VEC_PROG);
    
    setSIV(tmp);
}

#ifndef CONFIG_SMP_AMD64								/* XXX todo */

void
APIC::startSecondary(const apicId targetId, const uval8 vector)
{
    // set Target Processor
    uval32 icr_hi=APIC::getInterruptCmdHi() & ~APIC_ID_MASK;
    
    icr_hi |= targetId << (APIC_ID_BITOFFSET);
    err_printf("sending INIT to target APIC of %d\n", targetId);
    APIC::setInterruptCmdHi(icr_hi);
    
    // Send first INIT IPI: causes a RESET on Target Processor */
    uval32 icr_low=APIC::getInterruptCmdLo();
    icr_low &= 0xfff00000;
    APIC::setInterruptCmdLo(icr_low | 0x0000c500);
    
    // Spin on status end
    while (APIC::getInterruptCmdLo() & APIC_DELSTAT_MASK){
	err_printf("-");
    }
    
    // do an INIT IPI: this dasserts the RESET */    
    APIC::setInterruptCmdLo(icr_low | 0x00008500);
    
    APIC::wait(10000); 
    // Spin on status end
    while (APIC::getInterruptCmdLo() & APIC_DELSTAT_MASK) {
	err_printf("+");
    }
    
    // Now send STARTUP IPI: We send the startup IPI twice as
    // some bugs in the P5 cause the first one to be missed.
    // If the firsts one is recognized the second one will 
    // automatically be ignored by the remote processor
    
    //FIXME:  confirm in doc this vector calc
    //uval vector = (bootAddr >> 12) & 0xff;
    
    // Here is the first one
    APIC::setInterruptCmdLo(icr_low | 0x00000600 | vector);
    // Spin on status end
    while (APIC::getInterruptCmdLo() & APIC_DELSTAT_MASK);
    APIC::wait(200);
    
    // Here is the second one
    APIC::setInterruptCmdLo(icr_low | 0x00000600 | vector);
    // Spin on status end
    while (APIC::getInterruptCmdLo() & APIC_DELSTAT_MASK);
    APIC::wait(200);	
    err_printf("APIC::done start secondary apicid %x", targetId);
}
#endif // CONFIG_SMP_AMD64
