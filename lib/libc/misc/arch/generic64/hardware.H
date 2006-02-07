#ifndef __HARDWARE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __HARDWARE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hardware.H,v 1.5 2003/10/08 13:33:47 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: AMD86 implementation of hardware specific features
 *
 * **************************************************************************/

typedef uval InterruptState;

static __inline uval
hardwareInterruptsEnabled()
{
  return (0);
}

/*
 * For the future, for efficiency, the interface allows for a variable
 * to be passed with the saved state. Currently we don't use this to
 * ensure that we are not depending on a recursive implementation.
 */
static __inline void
disableHardwareInterrupts(InterruptState &/*is*/)
{
}

static __inline void
enableHardwareInterrupts(InterruptState &/*is*/)
{
}

static __inline void
disableHardwareInterrupts()
{
}

static __inline void
enableHardwareInterrupts()
{
}

static __inline SysTime
getClock()
{
  return (0);
}

/*
 * true if clock can be read in user mode
 */
const uval usermodeClock=1;

static __inline void
setClock(SysTime now)
{
}

uval getInstrCount();

static __inline SysTime
setDecrementer(SysTime now)
{
  return (now);
}

static __inline SysTime
maxDecrementer()
{
  return (0x7FFFFFFFULL);
}

/*
 * operations to read and write memory mapped bytes.  Addr is always
 * the real address of the memory mapped byte.
 * assumes that neither the stack nor the code is at the very end of
 * memory.
 *
 */

static __inline void
ioOutUchar(uval8* addr,uval8 val)
{
}

static __inline uval8
ioInUchar(uval8* addr)
{
  return (0);
}

static __inline void
ioOutUval32(uval32* addr,uval32 val)
{
}

static __inline uval32
ioInUval32(uval32* addr)
{
  return (0);
}

// byte reversed versions
static __inline void
ioOutUval32BR(uval32* addr,uval32 val)
{
}

static __inline uval32
ioInUval32BR(uval32* addr)
{
  return (0);
}

static __inline uval
LocalToGlobalAddress(uval vaddr)
{
  return (vaddr);
}

static __inline uval32
readRealUval32(uval32* addr)
{
  return (*addr);
}

