#ifndef __BASE_RANDOM_H_
#define __BASE_RANDOM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseRandom.H,v 1.3 2002/10/10 13:08:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Re-entrant random number generator
 * **************************************************************************/

#include <scheduler/Scheduler.H>

class BaseRandom {
    sval32 *frontPtr;              /* Front pointer.  */
    sval32 *rearPtr;              /* Rear pointer.  */
    sval32 *stateArray;             /* Array of state values.  */
    sval32 *endPtr;           /* Pointer behind state table.  */

    uval type;          /* Type of random number generator.  */
    uval degree;           /* Degree of random number generator.  */
    uval separation;           /* Distance between front and rear.  */
    void init(uval seed);

public:
    DEFINE_NOOP_NEW(BaseRandom);
    BaseRandom() { init((uval)Scheduler::SysTimeNow()); }
    BaseRandom(uval seed) { init(seed); }
    ~BaseRandom() {;}

    sval32 stateMem[64];

    sval32 getVal();

    // This one is way simple don't know why we use it
    // Speed maybe?
    static uval32 GetLC(uval32 *state) {
	uval32 result;
	result = ((*state * 1103515245) + 12345) & (uval32(~0) >> 1);
	*state = result;
	return result;
    }
};
#endif /* #ifndef __BASE_RANDOM_H_ */
