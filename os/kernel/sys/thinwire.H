#ifndef __THINWIRE_H_
#define __THINWIRE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire.H,v 1.23 2005/02/09 18:45:44 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: header file for communicating over a multiplexed
 * thin-wire connection
 * **************************************************************************/

#include <sys/types.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <sys/IOChan.H>

#define CONSOLE_CHANNEL	 0
#define GDB_CHANNEL	 1
#define IPCUT_CHANNEL    2
#define ZIGGYDEB_CHANNEL 3
#define IPSEL_CHANNEL    4
#define NUM_CHANNELS	 5

void turnOffThinwire();			// only available in kernel
uval thinwireTest();
struct SLock;

/* Channel object with direct access to thinwire, without ThinWireMgr */
struct ThinWireChan: public IOChan {
    DEFINE_PRIMITIVE_NEW(ThinWireChan);
    ThinWireChan(uval num): IOChan(num) {};
    virtual uval write(const char* buf, uval length, uval block = 1);
    virtual uval read(char* buf, uval length, uval block = 1);
    virtual uval isReadable() {
	return 1;
    }

    static void ClassInit(VPNum vp, MemoryMgrPrimitive *mem);
    static void thinwireExit();		// tell thinwire daemon to exit
    static sval32 thinwireSelect();	// gives back bit mask of channels

    static SLock* TWLock;
    static IOChan* twChannels[NUM_CHANNELS];
    static IOChan* port;
    static uval enabled;

    static void EmergWrite(char* buf, sval len = -1);
};

static inline uval
thinwireWrite(uval channel, const char *buf, uval length)
{
    return ThinWireChan::twChannels[channel]->write(buf, length);
}

static inline uval
thinwireRead(uval channel, char *buf, uval length)
{
    return ThinWireChan::twChannels[channel]->read(buf, length);
}






#endif /* #ifndef __THINWIRE_H_ */
