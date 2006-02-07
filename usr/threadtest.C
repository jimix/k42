/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: threadtest.C,v 1.9 2005/06/28 19:48:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A program which illustrates how SimpleThreads
 *                     are used.
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <misc/simpleThread.H>
#include <sys/systemAccess.H>


SysStatus child_func(void *arg)
{
    cprintf("I am a child, and my argument is %p.\n", arg);
    return (SysStatus)arg;
}

int main(int argc, char *argv[])
{
    NativeProcess();

    VPNum numVP;
    SysStatus rc;

    if (argc != 1) {
	cprintf("Usage: %s\n", argv[0]);
	return 8;
    }

    // Figure out how many processors there are:
    numVP = DREFGOBJ(TheProcessRef)->ppCount();
    cprintf("There are %ld processors\n", uval(numVP));

    // Create a virtual processor for each physical processor:
    for (VPNum vp = 1; vp < numVP; vp++) {
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), {});
    }

    SimpleThread *threads[numVP];

    // Create a thread on each processor:
    for (VPNum vp=0; vp < numVP; vp++) {
	cprintf("Spawning thread %ld\n", vp);
	threads[vp] = SimpleThread::Create(child_func, (void *)vp,
					   SysTypes::DSPID(0, vp));
	passert(threads[vp] != 0, err_printf("Thread creation failed\n"));
    }

    // Wait for those threads to finish:
    for (VPNum vp=0; vp < numVP; vp++) {
	SysStatus rc;
	rc = SimpleThread::Join(threads[vp]);
	cprintf("Thread %ld terminated, status = %ld\n", vp, rc);
    }

    return 0;
}
