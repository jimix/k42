/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lockTest.C,v 1.4 2000/05/11 11:29:10 rosnbrg Exp $
 *****************************************************************************/

#include "Lock.H"

class Bits : public BitStructure {
public:
    BIT_FIELD(1, field1, BIT_FIELD_START);
    BIT_FIELD(1, field2, field1);
    BIT_FIELD(1, field3, field2);
    BIT_FIELD(2, field4, field3);
    BIT_FIELD(3, field5, field4);
    BIT_FIELD(8, field6, field5);
    BIT_FIELD(16, field7, field6);
    BIT_FIELD(32, field8, field7);

    LOCK_BIT(field6, 7);
    WAIT_BIT(field8, 0);
};

int main(void)
{
    Bits b;

    /*
     * BitBLock
     */
    BitBLock<Bits> bitsBLock;

    bitsBLock.acquire(b);
    b.field1(1);
    b.field2(1);
    b.field3(1);
    b.field4(1);
    b.field5(1);
    b.field6(1);
    b.field7(1);
    b.field8(2); // 1 would clobber the lock bit
    bitsBLock.release(b);

    bitsBLock.init(b);
    if (bitsBLock.tryAcquire(b)) {
	b.field1(1);
	if (bitsBLock.isLocked()) {
	    bitsBLock.release(b);
	}
    }

    bitsBLock.init();
    bitsBLock.acquire();
    bitsBLock.get(b);
    b.field1(1);
    bitsBLock.set(b);
    bitsBLock.release();

    /*
     * BitSLock
     */
    BitSLock<Bits> bitsSLock;

    bitsSLock.acquire(b);
    b.field1(1);
    b.field2(1);
    b.field3(1);
    b.field4(1);
    b.field5(1);
    b.field6(1);
    b.field7(1);
    b.field8(2); // 1 would clobber the lock bit
    bitsSLock.release(b);

    bitsSLock.init(b);
    if (bitsSLock.tryAcquire(b)) {
	b.field1(1);
	if (bitsSLock.isLocked()) {
	    bitsSLock.release(b);
	}
    }

    bitsSLock.init();
    bitsSLock.acquire();
    bitsSLock.get(b);
    b.field1(1);
    bitsSLock.set(b);
    bitsSLock.release();

    /*
     * BLock
     */
    BLock bLock;

    bLock.acquire();
    bLock.release();
    bLock.init();
    if (bLock.tryAcquire()) {
	if (bLock.isLocked()) {
	    bLock.release();
	}
    }

    /*
     * SLock
     */
    SLock sLock;

    sLock.acquire();
    sLock.release();
    sLock.init();
    if (sLock.tryAcquire()) {
	if (sLock.isLocked()) {
	    sLock.release();
	}
    }

    return 0;
}
