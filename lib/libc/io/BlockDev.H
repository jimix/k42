#ifndef __BLOCK_DEV_H_
#define __BLOCK_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockDev.H,v 1.19 2005/08/30 19:06:10 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of interface that must be exported to
 *		       a *FileServer object (to provide callbacks for the
 *		       *FileServer object to call).
 * **************************************************************************/

#include <cobj/Obj.H>
class BlockDev;
typedef BlockDev** BlockDevRef;

class BlockDev : public Obj {
public:
    // These are the addressing modes, specifying the kind of address
    // is passed in in the srcAddr and dstAddr parameters:
    enum AddrMode { None     = 0x0, // Invalid
		    Physical = 0x1, // A physical address
		    Virtual  = 0x2, // A virtual address (addressable by us)
		    Shared   = 0x3};// An offset into a MemTrans area
    enum OpType { DevRead = 0,
		  DevWrite= 1 };

#ifndef _IN_STUBGEN
    /* definitions required by the shared memory transport */
    static const uval TRANSPORT_SIZE = 10*PAGE_SIZE; /* allocate 10 pages 
						     *  for the transport */
    static const uval TRANSPORT_ENTRY_UVALS = 2;  /* communication involves 
						   * 2 uvals */
#if 1
    static const uval TransportNumEntries = (TRANSPORT_SIZE-2*sizeof(uval))
	/(TRANSPORT_ENTRY_UVALS*sizeof(uval));
#else
    static const uval TransportNumEntries = 32;
#endif

    /* struct for information (ack of disk operation completion) being sent
     * through transport channel */
    struct CallBackRequest {
	uval err;
	uval token;
    };

    struct DiskOpRequest {
	uval type, addr, size, offset, token;
    };
#endif //#ifndef _IN_STUBGEN

    virtual SysStatus putBlock(__in uval physAddr,
			       __in uval size,
			       __in uval objOffset)=0;

    virtual SysStatus getBlock(__in uval physAddr,
			       __in uval size,
			       __in uval objOffset)=0;
#ifndef _IN_STUBGEN
    struct IOCompletion{
	DEFINE_NOOP_NEW(IOCompletion);
	virtual void complete(BlockDevRef ref, SysStatus err)=0;
	virtual ~IOCompletion() {};
    };
#else
    struct IOCompletion;
#endif // _IN_STUBGEN

    virtual SysStatus asyncOp(uval type, uval physAddr, uval len,
			      uval objOffset, IOCompletion *ioc)=0;
__xpublic:

    virtual __async SysStatus _IORequest(__in uval opType, __in uval srcAddr,
					 __in uval size, __in uval objOffset,
					 __in uval token, __XHANDLE xh)=0;

    /* this sets use mode (physical, virtual) and creates the shared memory
     * transport for the completion acks to be produced by the BlockDev object */
    virtual SysStatus _useMode(__in uval addrMode, __in ObjectHandle oh,
			       __out ObjectHandle &transpFROH,
			       __XHANDLE xhandle, __CALLER_PID pid)=0;

    /* FIXME: we're bloating the interface in a bad way. The _useMode already
     * deals with setting up shared memory transport (but only with the disk
     * client as consumer, the disk object in the kernel as producer). Now we
     * are adding one more interface just to create the transport for sending
     * requests from the client to the disk object in the kernel.
     * This is not combined with _useMode, because _useMode is invoked by
     * code in DiskClient (therefore, for synchronous requests),
     * which doesn't need to have this request transport set */
    virtual SysStatus _setRequestTransport(__in uval addrMode,
					   __in ObjectHandle oh,
					   __in uval transpSize,
					   __in uval transpEntryUvals,
					   __in uval transpNumEntries,
					   __XHANDLE xhandle, __CALLER_PID pid)=0;

    virtual __async SysStatus _startWrite(__in uval srcAddr,
					  __in uval objOffset,
					  __in uval len,
					  __XHANDLE xhandle)=0;

    virtual __async SysStatus _startFillPage(__in uval dstAddr,
					     __in uval objOffset,
					     __XHANDLE xhandle)=0;

    virtual __async SysStatus _frIsNotInUse(__in uval fileToken)=0;

    /* The following method is used to kick the consumer of requester so it
     * starts to retrieve requests from the SharedBuffer area */
    virtual __async SysStatus _startIO(__XHANDLE xhandle)=0;

    virtual SysStatus _putBlock(__in uval srcAddr,
				__in uval size,
				__in uval objOffset,
				__XHANDLE xhandle)=0;

    virtual SysStatus _getBlock(__in uval dstAddr,
				__in uval size,
				__in uval objOffset,
				__XHANDLE xhandle)=0;

    virtual SysStatus _registerCallback(__in ObjectHandle oh,
					__XHANDLE xhandle) = 0;



};


#endif /* #ifndef __BLOCK_DEV_H_ */

