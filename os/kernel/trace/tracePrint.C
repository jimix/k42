/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: tracePrint.C,v 1.46 2004/04/12 13:09:18 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of tracing facility
 * **************************************************************************/

#include "kernIncs.H"
#include <misc/baseStdio.H>
#include "bilge/SystemControl.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"
#include "trace/traceUtils.H"

void
traceSetEventPrintType()
{
    char tbuf[80];
    sval ch=0;
    sval i;
    char *maj;
    uval majInd;

    cprintf("choose option to set event type: [1-4]\n");
    cprintf(" 1. menu to choose a single major event ID to print\n");
    cprintf(" 2. enter mask of major event IDs to print\n");
    cprintf(" 3. set minor ID to print\n");
    cprintf(" 4. reset to defaults (all majors and all minors)\n");

    uval len = sizeof(tbuf) - 1;	// leave room for null terminator
    len = SystemControl::Read(tbuf,len);
    tbuf[len] = '\0';

    SysStatusUval ssu = baseAtoi(tbuf);
    if (_SUCCESS(ssu)) {
	ch = _SGETUVAL(ssu);
    }

    switch (ch) {
    case 1:
	cprintf("choose major ID to print:\n");
	for (i=0; i<TRACE_LAST_MAJOR_ID_CHECK; i++) {
	    cprintf(" %ld. %s\n", i, traceUnified[i].upName);
	}
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';

	ssu = baseAtoi(tbuf);
	if (_SUCCESS(ssu)) {
	    ch = _SGETUVAL(ssu);
	} else {
	    ch = -1;
	}

	if ((ch>=0) && (ch<TRACE_LAST_MAJOR_ID_CHECK)) {
	    traceMajorIDPrintMask = traceUnified[ch].mask;
	} else {
	    cprintf("unknown option\n");
	}
	break;
    case 2:
	cprintf("enter or'ed constants or single number\n");
	cprintf(" ex: 0x3 for test and mem events (FIXME - only 32 bits)\n");
	cprintf(" ex: TEST|MEM for test and mem events\n");
	cprintf("     where these came from TRACE_TEST_MASK in trace.H\n");
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';
	traceMajorIDPrintMask = 0;
	if ((tbuf[0] > '0') && (tbuf[0] < '9')) {
	    ssu = baseAtoi(tbuf);
	    if (_SUCCESS(ssu)) {
		traceMajorIDPrintMask = _SGETUVAL(ssu);
	    } else {
		traceMajorIDPrintMask = 0;
	    }
	}
	else {
	    majInd = 0;
	    maj = (char *)(&(tbuf[majInd]));
	    while (majInd < strlen(tbuf)) {
		switch (maj[0]) {

		case 'T':
		    traceMajorIDPrintMask |= TRACE_TEST_MASK;
		    majInd+=5;
		    break;
		case 'M':
		    if (maj[1] == 'E') {
			traceMajorIDPrintMask |= TRACE_MEM_MASK;
			majInd+=4;
		    }
		    else {
			if (strlen(maj) < 5) {
			    traceMajorIDPrintMask |= TRACE_MISC_MASK;
			    majInd+=5;
			}
			else {
			    traceMajorIDPrintMask |= TRACE_MISCKERN_MASK;
			    majInd+=9;
			}
		    }
		    break;
		case 'L':
		    traceMajorIDPrintMask |= TRACE_LOCK_MASK;
		    majInd+=5;
		    break;
		case 'U':
		    traceMajorIDPrintMask |= TRACE_USER_MASK;
		    majInd+=5;
		    break;
		case 'I':
		    traceMajorIDPrintMask |= TRACE_IO_MASK;
		    majInd+=3;
		    break;
		case 'A':
		    traceMajorIDPrintMask |= TRACE_ALLOC_MASK;
		    majInd+=6;
		    break;
		case 'P':
		    traceMajorIDPrintMask |= TRACE_PROC_MASK;
		    majInd+=5;
		    break;
		default:
		    cprintf("unknown class\n");
		    break;
		}
		maj = (char *)(&(tbuf[majInd]));
	    }
	}
	break;
    case 3:
	cprintf("enter MinorID to print: ");
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';

	ssu = baseAtoi(tbuf);
	if (_SUCCESS(ssu)) {
	    traceMinorIDPrint = _SGETUVAL(ssu);
	} else {
	    traceMinorIDPrint = 0;
	}

	break;
    case 4:
	traceMajorIDPrintMask = TRACE_ALL_MASK;
	traceMinorIDPrint = -1;
	break;
    default:
	cprintf("unknown option\n");
	break;
    }
}


void
tracePrintEvents(uval startIndex, uval stopIndex)
{
    uval i;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    uval64 *const trcArray = trcInfo->traceArray;

    for (i = startIndex;
	 i != stopIndex;
	 i = (i + TRACE_LENGTH_GET(trcArray[i])) & trcInfo->indexMask)
    {
	// FIXME get ticks per second correctly
	tracePrintEvent(&trcArray[i], 10000000);
    }
}

void
tracePrintLastEvents(uval numb)
{
    uval currentIndex, currentBufferStart, startIndex;
    uval bufferStart, bufferEnd;
    uval i, found;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    currentIndex = (trcCtrl->index & trcInfo->indexMask);
    currentBufferStart = TRACE_BUFFER_OFFSET_CLEAR(currentIndex);

    found = 0;
    startIndex = currentIndex;
    bufferStart = currentBufferStart;
    bufferEnd = currentIndex;

    for (;;) {
	if (trcCtrl->bufferCount[TRACE_BUFFER_NUMBER_GET(bufferStart)] !=
						   (bufferEnd - bufferStart)) {
	    // buffer isn't yet completed
	    break;
	}

	// buffer is complete, so we can include it in the output
	startIndex = bufferStart;

	// count the events in this buffer
	for (i = bufferStart; i < bufferEnd;
		    i += TRACE_LENGTH_GET(trcArray[i])) {
	    found++;
	}

	if (found >= numb) {
	    // we have enough events now
	    break;
	}

	// move to previous buffer
	if (bufferStart == 0) {
	    if (trcCtrl->buffersProduced == 0) {
		bufferStart = 0;
	    } else if (trcCtrl->buffersProduced < trcInfo->numberOfBuffers) {
		bufferStart = trcCtrl->buffersProduced - 1;
	    } else {
		bufferStart = (trcInfo->numberOfBuffers - 1) *
						    TRACE_BUFFER_SIZE;
	    }
	} else {
	    bufferStart -= TRACE_BUFFER_SIZE;
	}
	bufferEnd = bufferStart + TRACE_BUFFER_SIZE;

	if (bufferStart == currentBufferStart) {
	    // we're back to the beginning
	    err_printf("tracing warning: not enough valid events\n");
	    break;
	}
    }

    // skip events that weren't requested
    while (found > numb) {
	startIndex = (startIndex +
	      TRACE_LENGTH_GET(trcArray[startIndex])) & trcInfo->indexMask;
	found--;
    }

    // print the rest
    tracePrintEvents(startIndex, currentIndex);
}

void
tracePrintLast20Events()
{
    tracePrintLastEvents(20);
}

void
tracePrintBuffers(uval choice)
{
    uval len, numb;
    char tbuf[80];
    sval c;
    SysStatusUval ssu;
    uval startIndex, stopIndex;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;

    if (choice == 0) {
	cprintf("current trace index 0x%llx (buffer 0x%llx, offset 0x%llx)\n",
		trcCtrl->index,
		TRACE_BUFFER_NUMBER_GET(trcCtrl->index),
		TRACE_BUFFER_OFFSET_GET(trcCtrl->index));
	cprintf("    buffersProduced: 0x%lx, buffersConsumed: 0x%lx\n",
		trcCtrl->buffersProduced,
		trcCtrl->buffersConsumed);
	if (traceMajorIDPrintMask != TRACE_ALL_MASK) {
	    cprintf(" printing only major IDs of: ");
	    if (traceMajorIDPrintMask & TRACE_TEST_MASK) cprintf("TEST ");
	    if (traceMajorIDPrintMask & TRACE_MEM_MASK) cprintf("MEM ");
	    if (traceMajorIDPrintMask & TRACE_LOCK_MASK) cprintf("LOCK ");
	    if (traceMajorIDPrintMask & TRACE_USER_MASK) cprintf("USER ");
	    if (traceMajorIDPrintMask & TRACE_IO_MASK) cprintf("IO ");
	    if (traceMajorIDPrintMask & TRACE_ALLOC_MASK) cprintf("ALLOC ");
	    if (traceMajorIDPrintMask & TRACE_MISC_MASK) cprintf("MISC ");
	    if (traceMajorIDPrintMask & TRACE_PROC_MASK) cprintf("PROC ");
	    if (traceMajorIDPrintMask & TRACE_DBG_MASK) cprintf("DBG ");
	    if (traceMajorIDPrintMask & TRACE_HWPERFMON_MASK)cprintf("HWPERF ");
	    if (traceMajorIDPrintMask & TRACE_LINUX_MASK) cprintf("LK ");
	    cprintf("\n");
	}
	if (traceMinorIDPrint != -1) {
	    cprintf(" printing only minor ID %ld\n", traceMinorIDPrint);
	}
	cprintf("\n");

	cprintf("choose option to print trace buffers: [1-5]\n");
	cprintf(" 1. last 20 events\n");
	cprintf(" 2. last n events\n");
	cprintf(" 3. all events from current buffer\n");
	cprintf(" 4. all events from buffer n\n");
	cprintf(" 5. all events\n");
	cprintf(" 6. set type of events to print\n");
	cprintf(" 7. set trace mask\n");
	cprintf(" 8. trace counters\n");
	cprintf(" 9. trace counters and reset\n");

	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';
	c = tbuf[0];
    } else {
	if (choice > 9) {
	    cprintf("error value wrong\n");
	}
	c = '0'+choice;
    }

    switch (c) {
    case '1':
	tracePrintLastEvents(20);
	break;
    case '2':
	cprintf("enter the number of event you want printed:\n");
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf, len);
	tbuf[len] = '\0';
	ssu = baseAtoi(tbuf);
	if (_SUCCESS(ssu)) {
	    numb = _SGETUVAL(ssu);
	} else {
	    numb = 0;
	}
	tracePrintLastEvents(numb);
	break;
    case '3':
	stopIndex = trcCtrl->index & trcInfo->indexMask;
	startIndex = TRACE_BUFFER_OFFSET_CLEAR(stopIndex);
	if (trcCtrl->bufferCount[TRACE_BUFFER_NUMBER_GET(startIndex)] ==
						    (stopIndex - startIndex)) {
	    tracePrintEvents(startIndex, stopIndex);
	} else {
	    cprintf("Current buffer has incomplete events.\n");
	}
	break;
    case '4':
	cprintf("enter the number of buffer you would like printed:\n");
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf, len);
	tbuf[len] = '\0';
	ssu = baseAtoi(tbuf);
	if (_SUCCESS(ssu)) {
	    numb = _SGETUVAL(ssu);
	} else {
	    numb = 0;
	}
	startIndex = numb * TRACE_BUFFER_SIZE;
	stopIndex = startIndex + TRACE_BUFFER_SIZE;
	if (trcCtrl->bufferCount[TRACE_BUFFER_NUMBER_GET(startIndex)] ==
						    (stopIndex - startIndex)) {
	    tracePrintEvents(startIndex, stopIndex);
	} else {
	    cprintf("Buffer 0x%lx has incomplete events.\n", numb);
	}
	break;
    case '5':
	tracePrintLastEvents(trcInfo->numberOfBuffers * TRACE_BUFFER_SIZE);
	break;
    case '6':
	traceSetEventPrintType();
	tracePrintBuffers(0);
	break;
    case '7':
	cprintf("set mask NYI\n");
	break;
    case '8':
    case '9':
	cprintf("trace counters dump NI:\n");
	//for (numb = 0; numb < TraceAutoCount::MAX; numb++ ) {
	//cprintf("%ld: ", numb);
	//TraceAutoCount::dumpCounterPair(TraceAutoCount::CtrIdx(numb));
	//}
	//if (c=='9') TraceAutoCount::initCounters();
	break;
    default:
	cprintf("unknown option\n");
	break;
    }
}
