#ifndef __DISPATCHER_H_
#define __DISPATCHER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Dispatcher.H,v 1.41 2004/09/20 18:53:03 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the base interface between the kernel
 *     and a user-mode dispatcher. For documentation on how async
 *     messages are implemented, see os/kernel/sys/README.html#async-msg
 * **************************************************************************/

#include "sys/VolatileState.H"
#include "sys/SoftIntr.H"

struct DispatcherCore {
    static const uval NUM_PGFLT_ID_WORDS = 2;
    static const uval NUM_PGFLT_IDS = (NUM_PGFLT_ID_WORDS * 64);
    static const uval NUM_IPC_RETRY_IDS = 64;

    DispatcherID dspid;
    SoftIntr interrupts;
    uval hasWork;
    SysStatus ipcFaultReason;
    uval ipcRetryID;
    uval trapDisabledSave;	// disabled status at time of trap
    uval _trapStateOffset;
    uval _userStateOffset;
    uval64 pgfltCompleted[NUM_PGFLT_ID_WORDS];
    uval64 ipcRetry;
    VolatileState _state[3];

    ProcessID processID;	// store process id for debugging
    char progName[64];		// store program name here for debugging

    void initCore(DispatcherID dspidArg) {
	dspid = dspidArg;
	interrupts.init();
	hasWork = 0;
	ipcFaultReason = 0;
	ipcRetryID = 0;
	trapDisabledSave = 0;
	_trapStateOffset = 0;
	_userStateOffset = 0;
	for (uval i = 0; i < NUM_PGFLT_ID_WORDS; i++) pgfltCompleted[i] = 0;
	ipcRetry = 0;
	processID = ProcessID(-1);
	progName[0] = '\0';
    }

    DispatcherID getDspID() {return dspid;}

    void storeProgInfo(ProcessID newProcID, char *newName) {
	tassertSilent((newName != NULL) && (newName[0] != '\0'), BREAKPOINT);
	processID = newProcID;
	strncpy(progName, newName, 64);
	progName[63] = '\0';
    }

    VolatileState *userStatePtr() {
	// The returned pointer is valid while the dispatcher remains
	// disabled.
	return (VolatileState *) (((uval) this) + _userStateOffset);
    }

    VolatileState *trapStatePtr() {
	// The returned pointer is valid while the dispatcher remains
	// disabled and no new traps are delivered.  Traps will be
	// delivered (and _trapStateOffset may change) even while we're
	// disabled.  We just have to know that we won't do anything
	// to cause a new trap while we're using the returned pointer.
	return (VolatileState *) (((uval) this) + _trapStateOffset);
    }
};

class AsyncBuffer {
public:
    static const uval HDR_LENGTH = 3;
    static const uval MSG_LENGTH = 6;
    static const uval MAX_LENGTH = (HDR_LENGTH + MSG_LENGTH);
    /*
     * The Dispatcher structure must fit in a page because of the way it's
     * shared between kernel and user address spaces.  (We can change this
     * limit if necessary.)  The structure consists of a DispatcherCore and
     * two AsyncBuffers.  BUF_SIZE is chosen to use all the available space.
     */
    static const uval BUF_SIZE =
	(((PAGE_SIZE - sizeof(DispatcherCore))	// available space
	    / sizeof(uval))			// ... in uvals
	    / 2)				// ... per AsyncBuffer
	    - 2;				// ... less two control words
    /*
     * The nominal buffer capacity is made less than the actual size so that
     * the message at the end of the buffer can be kept contiguous when it
     * would logically wrap to the beginning.
     */
    static const uval CAPACITY = BUF_SIZE - (MAX_LENGTH - 1);
protected:
    uval delivered;
    uval accepted;
    uval buf[BUF_SIZE];

    static const uval LENGTH_BITS = 8;
    static const uval METHOD_BITS = (((sizeof(uval)) * 8) - LENGTH_BITS);

public:

    void init() { delivered = 0; accepted = 0; }

    uval isEmpty() { return (accepted == delivered); }

    static void PackMethodAndLength(uval &word, uval method, uval length) {
	word = ((length << METHOD_BITS) |
		    (method & ((uval(1) << METHOD_BITS) - 1)));
    }

    static void UnpackMethodAndLength(uval &word, uval &method,
				      volatile uval &length) {
	length = ((word >> METHOD_BITS) & ((uval(1) << LENGTH_BITS) - 1));
	method = (word & ((uval(1) << METHOD_BITS) - 1));
    }
};

class AsyncBufferLocal : public AsyncBuffer {
public:

    SysStatus fetchMsg(ProcessID &badge, XHandle &xh, uval &method,
		       volatile uval &length, void *data) {
	// FetchMsg() is used (sometimes) to copy a message directly to the
	// PPC page.  It's crucial that the length be stored before the data
	// is copied.

	// Force order to enable cache line optimization in message delivery.
	// Things might get reset to zero, in which case we are guaranteed that
	// there is a message.  Hence, we *need* to see the correct value
	// for accepted, but can handle a huge bogus value for delivered.
	uval lDelivered1 = * ((volatile uval *) &delivered);
	uval lAccepted   = * ((volatile uval *) &accepted);
	uval lDelivered2 = * ((volatile uval *) &delivered);
 	if (lAccepted >= lDelivered1) {
 	    return -1;
 	}
 	uval index = lAccepted % CAPACITY;
 	badge = buf[index];
 	xh = buf[index + 1];
 	UnpackMethodAndLength(buf[index + 2], method, length);
 	tassert(length < (MSG_LENGTH*sizeof(uval)),
 		err_printf("async message exceeds max size\n"));
	// Must check before updating accepted, since afterwards
	// kernel may reset both to zero.
 	lAccepted += (HDR_LENGTH + NUM_UVALS(length));
 	tassert(lAccepted <= lDelivered2,
 		err_printf("garbled async message\n"));
 	memcpy(data, &buf[index + 3], length);
 	accepted = lAccepted;
 	return 0;
    }

    SysStatus storeMsg(ProcessID badge, XHandle xh, uval method,
		       uval length, void *data) {
	uval const required = HDR_LENGTH + NUM_UVALS(length);

	if (required > MAX_LENGTH) {
	    return _SERROR(2531, 0, EMSGSIZE);
	}

	uval const avail = CAPACITY - (delivered - accepted);

	if (required > avail) {
	    return _SERROR(2532, 0, EBUSY);
	}

	if (accepted == delivered) {
	    /*
	     * In order to improve cache utilization for buf, we
	     * reset the bytes-delivered and bytes-accepted counters to
	     * zero whenever we find the buffer empty.
	     * For local case, this is executed atomically w.r.t. reader.
	     */
	    accepted = 0;
	    delivered = 0;
	}

	uval const index = delivered % CAPACITY;

	buf[index] = (uval) badge;
	buf[index + 1] = (uval) xh;
	PackMethodAndLength(buf[index + 2], method, length);
	memcpy(&buf[index + 3], data, length);

	delivered += required;
	return 0;
    }
};

class AsyncBufferRemote : public AsyncBuffer {
public:

    SysStatus fetchMsg(ProcessID &badge, XHandle &xh, uval &method,
		       volatile uval &length, void *data) {
	// FetchMsg() is used (sometimes) to copy a message directly to the
	// PPC page.  It's crucial that the length be stored before the data
	// is copied.

	SyncAfterAcquire();		// sync w.r.t interrupt bit
 	if (accepted >= delivered) {
 	    return -1;
 	}
	SyncAfterAcquire();
 	uval index = accepted % CAPACITY;
 	badge = buf[index];
 	xh = buf[index + 1];
 	UnpackMethodAndLength(buf[index + 2], method, length);
 	tassert(length < (MSG_LENGTH * sizeof(uval)),
 		err_printf("async message exceeds max size\n"));
 	memcpy(data, &buf[index + 3], length);
	SyncBeforeRelease();
 	accepted += (HDR_LENGTH + NUM_UVALS(length));
 	tassert(accepted <= delivered,
 		err_printf("garbled async message\n"));
 	return 0;
    }

    SysStatus storeMsg(ProcessID badge, XHandle xh, uval method,
		       uval length, void *data) {
	uval const required = HDR_LENGTH + NUM_UVALS(length);

	if (required > MAX_LENGTH) {
	    return _SERROR(1109, 0, EMSGSIZE);
	}

	uval const avail = CAPACITY - (delivered - accepted);
	if (required > avail) {
	    return _SERROR(1110, 0, EBUSY);
	}
	SyncAfterAcquire();

	uval const index = delivered % CAPACITY;

	buf[index] = (uval) badge;
	buf[index + 1] = (uval) xh;
	PackMethodAndLength(buf[index + 2], method, length);
	memcpy(&buf[index + 3], data, length);

	SyncBeforeRelease();
	delivered += required;
	SyncBeforeRelease(); // to gaurantee delivered set before interrupt bit
	return 0;
    }
};

struct Dispatcher : public DispatcherCore {
    AsyncBufferLocal asyncBufferLocal;
    AsyncBufferRemote asyncBufferRemote;

    void init(DispatcherID dspidArg) {
	initCore(dspidArg);
	asyncBufferRemote.init();
	asyncBufferLocal.init();
    }
};

#endif /* #ifndef __DISPATCHER_H_ */
