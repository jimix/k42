#ifndef __CPUDOMAIN_ANNEX_H_
#define __CPUDOMAIN_ANNEX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CPUDomainAnnex.H,v 1.21 2004/09/15 17:39:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines exception-level cpu scheduling machinery.
 * **************************************************************************/

#include "CPUContainer.H"
#include "ProcessAnnex.H"
#include "trace/traceException.h"

class CPUDomainAnnex : public CPUContainer {
public:		// must be public to be used in embedded class below
    enum {
	// bits must sum to 64
	PCLASS_BITS = 4,
	EXPONENT_BITS = 40,
	MANTISSA_BITS = 20
    };

private:
  /*
   * The following union is an attempt to overlay the priority
   * field with its component parts. Depends upon how bit fields
   * are packed in a uval64.  The objective is that a priority is
   * composed of three parts.  The most important is the priority
   * class (pclass), then the exponent, and finally the mantissa.
   * We want to be able to set these fields separately, but
   * always compare them as an ordered triplet.
   */

    union priority_value {
	uval64 whole;
	struct {
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#define __BIG_ENDIAN__   // defined BIG_ENDIAN by default if neither is defined
#endif /* #if !defined(__BIG_ENDIAN__) && ... */
#if defined(__BIG_ENDIAN__)
	    uval64 pclass:PCLASS_BITS;
	    uval64 exponent:EXPONENT_BITS;
	    uval64 mantissa:MANTISSA_BITS;
#elif defined(__LITTLE_ENDIAN__)
	    uval64 mantissa:MANTISSA_BITS;
	    uval64 exponent:EXPONENT_BITS;
	    uval64 pclass:PCLASS_BITS;
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */
	} part;
    } priority;
    SysTime consumedTicks;
    SysTime quantum;
    SysTime pulseInterval;
    uval drag;
    uval weight;
    ProcessAnnex *currentPA;
    VPNum pp;

public:
    DEFINE_PINNEDLOCALSTRICT_NEW(CPUDomainAnnex);
    DEFINE_PRIMITIVE_NEW(CPUDomainAnnex);

    static uval TickScale(SysTime inflationInterval);
    static SysStatus Create(CPUContainerRef &ref,
			    uval pclass, uval weight,
			    uval quantumMicrosecs, uval pulseMicrosecs);
    void init(uval pclass, uval wght,
	      uval quantumMicrosecs, uval pulseMicrosecs);

    uval hasHigherPriorityThan(CPUDomainAnnex *cda) {
	return (priority.whole < cda->priority.whole);
    }

    uval getPriorityClass() { return priority.part.pclass; }
    uval64 getPriority() { return priority.whole; }
    SysTime getQuantum() { return quantum; }
    SysTime getPulseInterval() { return pulseInterval; }
    uval getWeight() { return weight; }
    VPNum getPP() { return pp; }

    void adjustPriority(uval inflation, uval tickScale, SysTime delta) {
	uval64 mantissa;
	mantissa = ((priority.part.mantissa
		     >> (inflation - priority.part.exponent))
		    + ((drag * delta) >> tickScale));
	tassertMsg((mantissa >> MANTISSA_BITS) == 0, "mantissa overflow.\n");
	tassertMsg((inflation >> EXPONENT_BITS) == 0, "exponent overflow.\n");
	priority.part.mantissa = mantissa;
	priority.part.exponent = inflation;
    }

    void flushQuantum(uval inflation, uval tickScale) {
	adjustPriority(inflation, tickScale, consumedTicks);
	consumedTicks = 0;
    }

    void adjustQuantum(uval inflation, uval tickScale, SysTime delta) {
	consumedTicks += delta;
	if (consumedTicks >= quantum) {
	    TraceOSExceptionQuantumExpired(currentPA->commID);
	    flushQuantum(inflation, tickScale);
	    currentPA->resetPulse(0);
	    currentPA = currentPA->cpuDomainNext;
	}
    }

    uval hasFreshQuantum() {
	return (consumedTicks == 0);
    }

    SysTime getTimeout() {
	tassertMsg(consumedTicks <= quantum, "Excessive consumedTicks.\n");
	return (quantum - consumedTicks);
    }

    void addProcessAnnex(ProcessAnnex *pa);
    void removeProcessAnnex(ProcessAnnex *pa);
    uval stillRunnable();

    uval isCurrent(ProcessAnnex *pa) {
	return (pa == currentPA);
    }

    void makeCurrent(ProcessAnnex *pa) {
	/*
	 * pa should already be on this domain's ring.  We simply
	 * make it current.
	 */
	tassertMsg(pa->cpuDomainAnnex == this, "Wrong domain.\n");
	tassertMsg(pa->cpuDomainNext != NULL, "PA not in a ring.\n");
	currentPA = pa;
    }

    ProcessAnnex *getCurrentProcessAnnex();

    /*
     * Exported interfaces.
     */
    virtual SysStatus _setPriorityClass(__in uval pclass);
    virtual SysStatus _setWeight(__in uval wght);

    virtual SysStatus _createFirstDispatcher(__in ObjectHandle childOH,
					     __in ProcessID parentPID,
					     __in EntryPointDesc entry,
					     __in uval dispatcherAddr,
					     __in uval initMsgLength,
					     __inbuf(initMsgLength)
					     char *initMsg);

    virtual SysStatus _createDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid,
				        __in EntryPointDesc entry,
				        __in uval dispatcherAddr,
				        __in uval initMsgLength,
				        __inbuf(initMsgLength) char *initMsg);

    virtual SysStatus _detachDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid);
    virtual SysStatus _attachDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid);
};

#endif /* #ifndef __CPUDOMAIN_ANNEX_H_ */
