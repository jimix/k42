/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: gdb-stub.C,v 1.83 2005/07/18 12:32:09 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * GDB stub for PowerPC - derived from a number of sources, all of which are
 * either not COPYRIGHTED or are offered for free use of any kind
 * **************************************************************************/

/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *  Also, need to assign exceptionHook and oldExceptionHook.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the sval array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   (signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info>::<characters representing the command or response>
 * <checksum>   ::< two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

/*
 *
 * Modified for PowerPC Kitchawan.
 * Because the PSIM simulator is hard wired to special case all twge a,a
 * we need a modified gdb for normal debugging.  gdb-user is modified to
 * use twe 2,2 as its breakpoint instruction
 */

#include "sys/sysIncs.H"
#include "GDBIO.H"
#include <sys/ppccore.H>
#include <misc/hardware.H>
#include <sys/Dispatcher.H>
#include <sys/KernelInfo.H>
#include <misc/utilities.H>
#include <misc/linkage.H>
#include <scheduler/Scheduler.H>
#include <misc/arch/powerpc/trap.h>
#include <sync/SLock.H>

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/

/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 2048

static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];
static uval error;
static ExtRegs SavedExtRegs;
static uval SavedPrimitivePPCFlag;

static sval remote_debug = 0;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */


static const char hexchars[]="0123456789abcdef";

/* see gdb-4.16/sim/ppc/registers.h and/or tm_rs6000.h*/
/**
 ** And the registers proper
 **/
struct _registers {
    uval64 gpr[32];
    uval64 fpr[32];
    uval64 pc;
    uval64 ps;
    uval32 cr;
    uval64 lr;
    uval64 ctr;
    uval32 xer;
    uval32 fpscr;
} __attribute__((packed));

/* We can't get strcpy from ld.so */
#define strcpy __strcpy
static char *__strcpy(char *dest, const char *src)
{
    return (char*)memcpy(dest,src, strlen(src)+1);
}

static struct _registers registers;

/* Number of bytes of registers.  */
#define NUMREGBYTES (sizeof(_registers))

static sval
hex(char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

static char *
getPacket(void)
{
    unsigned char checksum;
    unsigned char xmitcsum;
    sval i;
    sval count;

    for (;;) {
	count = GDBIO::GDBRead(remcomInBuffer, BUFMAX-2);
	if (count == 0) {
	    /*
	     * read() has a window during which it's a BAD IDEA to
	     * interrupt execution and save the state of the computation.  To
	     * lessen the probability of hitting that window, we waste some
	     * time here.
	     */
	    int x = 0;
	    for (i = 0; i < 100; i++) x+=strlen("");
	    continue;
	}

#define CMAX 40
	if (remote_debug) {
	    if (count < CMAX) {
		remcomInBuffer[count] = 0;	// terminate print
		cprintf("G%ld %s\n",count,remcomInBuffer);
	    } else {
		char temp=remcomInBuffer[CMAX];
		remcomInBuffer[CMAX] = 0;
		cprintf("G%ld %s...\n",count,remcomInBuffer);
		remcomInBuffer[CMAX] = temp;
	    }
	}

	if ((count >= 4) &&
	    (remcomInBuffer[0] == '$') &&
	    (remcomInBuffer[count-3] == '#')) {
	    remcomInBuffer[count-3] = '\0';
	    xmitcsum = (hex(remcomInBuffer[count-2]) << 4) +
		hex(remcomInBuffer[count-1]);
	    checksum = 0;
	    for (i = 1; i < count-3; i++) {
		checksum += remcomInBuffer[i];
	    }
	    if (checksum == xmitcsum) {
		GDBIO::GDBWrite("+", 1);
		if ((count >= 7) && (remcomInBuffer[3] == ':')) {
		    GDBIO::GDBWrite(remcomInBuffer+1, 2);
		    return (remcomInBuffer + 4);
		} else {
		    return (remcomInBuffer + 1);
		}
	    }
	}

	GDBIO::GDBWrite("-", 1);
    }
}

static char *
putPacketStart(void)
{
  remcomOutBuffer[0] = '$';
  remcomOutBuffer[1] = '\0';
  return (remcomOutBuffer + 1);
}

static void
putPacketFinish(void)
{
  unsigned char checksum;
  sval count;
  char ch;

  /*  $<packet info>#<checksum>. */


  checksum = 0;
  for (count = 1; (ch = remcomOutBuffer[count]) != '\0'; count++) {
    checksum += ch;
  }
  remcomOutBuffer[count + 0] = '#';
  remcomOutBuffer[count + 1] = hexchars[checksum >> 4];
  remcomOutBuffer[count + 2] = hexchars[checksum % 16];

  if (remote_debug) {
      if (count < CMAX-3) {
	  remcomOutBuffer[count+3] = 0;	// terminate print
	  cprintf("P%ld %s\n",count+3,remcomOutBuffer);
      } else {
	  char temp=remcomOutBuffer[CMAX+3];
	  remcomOutBuffer[CMAX+3] = 0;
	  cprintf("P%ld %s...\n",count+3,remcomOutBuffer);
	  remcomOutBuffer[CMAX+3] = temp;
      }
  }

  do {
      GDBIO::GDBWrite(remcomOutBuffer, count + 3);
      while (GDBIO::GDBRead(&ch, 1) < 1) /* no-op */;
  } while (ch != '+');
  remcomOutBuffer[count] = 0;
}



static void
debug_error(char * format, char * parm)
{
    if (remote_debug) err_printf(format,parm);
}

/* flag set if a mem fault is possible.  Faults are handled by
 * skipping the faulting instruction and setting mem_err to 1
 */
static volatile uval gdb_mem_fault_possible = 0;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile sval mem_err = 0;

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
static sval
get_char (char *addr)
{
  return *addr;
}

static void
set_char (char *addr, sval val)
{
  *addr = val;
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
static char*
mem2hex(char* mem, char* buf, sval count, sval may_fault)
{
      sval i;
      unsigned char ch;

      //kludge - extRegsLocal has been saved - read saved version
      if ((mem >= ((char *)(&extRegsLocal))) &&
				  (mem < ((char *)((&extRegsLocal) + 1)))) {
	  mem += (((char *)(&SavedExtRegs)) - ((char *)(&extRegsLocal)));
      }

      if (may_fault) gdb_mem_fault_possible = 1;
      for (i=0;i<count;i++) {
          ch = get_char (mem++);
	  if (may_fault && mem_err) break;
          *buf++ = hexchars[ch >> 4];
          *buf++ = hexchars[ch % 16];
      }
      *buf = 0;
      if (may_fault) gdb_mem_fault_possible = 0;
      return (buf);
}


/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char*
hex2mem(char* buf, char* mem, sval count, sval may_fault)
{
      sval i;
      unsigned char ch;

      /*
       * Mega Kludge
       * Debugger is getting into trouble putting breakpoints in
       * stuff it uses like strcpy.  Eventually, we'll build it as a
       * self contained library with copies of everything.
       * For now, just ignore requests to change dangerous locations.
       * Since this is powerpc TOC linkage, address of function code
       * is in fact in first word of the function descriptor.
       */
#define PRECIOUS(x) {(char **) x, #x}
      struct {
	  char ** fptr;
	  char * fname;
      } static const Precious[] = {
	  PRECIOUS(memcpy),  // gcc calls this for structure copies
	  PRECIOUS(strlen),  // we use these here
	  PRECIOUS(strcpy),
	  PRECIOUS(memset),
	  PRECIOUS(err_printf)
      };


      for (i = 0;(uval)i < sizeof(Precious) / sizeof(Precious[0]); i++) {
	  char* p;
	  p = *(Precious[i].fptr);	// code pointer from descriptor
	  if (mem >= p && mem < (p + 64)) {
	      // 64 bytes seems good since gdb will set breakpoint
	      // afters the prologue
	      if (remote_debug) {
		  err_printf("gdb-stub: skipping write to %p <%s+%lu>\n",
			     mem, Precious[i].fname, (uval)mem - (uval)p);
	      }
	      return (mem);
	  }
      }

      //kludge - extRegsLocal has been saved - write saved version
      if ((mem >= ((char *)(&extRegsLocal))) &&
				  (mem < ((char *)((&extRegsLocal) + 1)))) {
	  mem += (((char *)(&SavedExtRegs)) - ((char *)(&extRegsLocal)));
      }

      if (may_fault) gdb_mem_fault_possible = 1;
      for (i = 0;i < count; i++) {
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);
          set_char(mem, ch);
	  if (may_fault && mem_err) break;
	  // force data to memory and clear instruction cache in case this is
	  // an instruction.  the change rate on this path is small enough
	  // that the gross inefficiency of this approach is OK.
	  asm("dcbst 0,%0;sync;icbi 0,%0" : : "r" (mem));
	  mem++;
      }
      // If instructions were modified - guarantee that modifications have
      // reached the processor.
      asm("isync");
      if (may_fault) gdb_mem_fault_possible = 0;
      return (mem);
}

static uval64
memword(uval32* wordPtr)
{
    uval32 word;
    gdb_mem_fault_possible = 1;
    mem_err = 0;
    word = *wordPtr;			// if this faults, mem_err is set to 1
    gdb_mem_fault_possible = 0;		// and execution continues here
    return word;
}

/* this function takes the PowerPC exception vector and attempts to
 *  translate this number into a unix compatible signal value.  See
 *  trap.h for values.
 *  This code also adjusts the PC to skip twe 0,0 - we use that
 *  for breakpoint.  I can't see how to get gdb to skip it.
 */

static sval
computeSignal(uval exceptionVector)
{

    sval sigval;
    uval srr1 = registers.ps;
    switch (exceptionVector) {
    case EXC_PGM:
	switch (srr1 & PSL_TRAP_FIELD) {
	case PSL_TRAP_FE:
	    sigval = 8; break;  /* floating point error */
	case PSL_TRAP_IOP:
	case PSL_TRAP_PRV:
	    sigval = 4; break;  /* invalid opcode */
	case PSL_TRAP:
	{
#define USER_BREAKPOINT 0x7fe00008  /* (tw 31,0,0) trap used for break */
	    uval32 op = memword((uval32*)(registers.pc));
	    if (!mem_err && (op == USER_BREAKPOINT)) {
		registers.pc += 4;
	    }
	    sigval = 5; break;  /* trap */
	}
	default:
	    sigval = 7;
	}
	break;
    case EXC_DSI:
    case EXC_ISI:
	sigval = 11;break;      /* segv */
    case EXC_TRC:
    case EXC_ALI:
	sigval = 5;break;       /* debug */
    default:
	sigval = 7;         /* "software generated"*/
    }
    return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static sval
hexToInt(char **ptr, sval *intValue)
{
    sval numChars = 0;
    sval hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex(**ptr);
        if (hexValue >=0)
        {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}

/*
 * Keep trap information here to help us figure out what happened when we
 * take recursive traps, etc.
 */
static uval gdb_trapPC = uval(-1);
static uval gdb_trapLR = uval(-1);
static uval gdb_trapSP = uval(-1);
static uval gdb_vector = uval(-1);
static uval gdb_trapInfo = uval(-1);
static uval gdb_trapAuxInfo = uval(-1);

/*
 * This function does all command procesing for interfacing to gdb.
 */
static void
handle_exception(uval exceptionVector)
{
  sval   sigval;
  sval   addr, length;
  char * ptr;
  sval   newPC;
  char * inBuffer=0;
  char * outBuffer=0;
  uval	 i = 0;

  if (remote_debug) cprintf("vector=0x%lx, sr=0x%lx, pc=0x%lx lr=0x%lx\n",
			    exceptionVector,
			    (uval)registers.ps,
			    (uval)registers.pc,
			    (uval)registers.lr);
  sigval = computeSignal(exceptionVector);

#if 0 /* turn this on to see SLB contents at every exception */
  unsigned int i;
  uval v, e, w, f;
  for (i = 0; i < 64; i += 2) {
   asm volatile ("slbmfev %0,%2; slbmfee %1,%2" : "=&r"(v),"=&r"(e) : "r"(i));
   asm volatile ("slbmfev %0,%2; slbmfee %1,%2" : "=&r"(w),"=&r"(f) : "r"(i+1));
   err_printf("SLB%x %lx %lx %lx %lx\n", i, v, e, w, f);
  }
#endif /* #if 0 */

  if (GDBIO::IsConnected()) {
    /* reply to host that an exception has occurred */
    outBuffer = putPacketStart();
    outBuffer[0] = 'S';
    outBuffer[1] =  hexchars[sigval >> 4];
    outBuffer[2] =  hexchars[sigval % 16];
    outBuffer[3] =  '\0';
    putPacketFinish();
  } else {
      switch (exceptionVector) {
      case EXC_RSVD:
	  err_printf("GDB got trap: Reserved\n");
	  break;
      case EXC_RST:
	  err_printf("GDB got trap: Reset\n");
	  break;
      case EXC_MCHK:
	  err_printf("GDB got trap: Machine Check\n");
	  break;
      case EXC_DSI:
	  err_printf("GDB got trap: Data Storage Interrupt\n");
	  break;
      case EXC_ISI:
	  err_printf("GDB got trap: Instruction Storage Interrupt\n");
	  break;
      case EXC_EXI:
	  err_printf("GDB got trap: External Interrupt\n");
	  break;
      case EXC_ALI:
	  err_printf("GDB got trap: Alignment Interrupt\n");
	  break;
      case EXC_PGM:
	  err_printf("GDB got trap: Program Interrupt\n");
	  break;
      case EXC_FPU:
	  err_printf("GDB got trap: Floating-point Unavailable\n");
	  break;
      case EXC_DEC:
	  err_printf("GDB got trap: Decrementer Interrupt\n");
	  break;
      case EXC_SC:
	  err_printf("GDB got trap: System Call\n");
	  break;
      case EXC_TRC:
	  err_printf("GDB got trap: Trace\n");
	  break;
      case EXC_FPA:
	  err_printf("GDB got trap: Floating-point Assist\n");
	  break;
      };
      err_printf("vector=0x%lx, sr=0x%lx, pc=0x%lx lr=0x%lx\n",
		 exceptionVector,
		 (uval)registers.ps,
		 (uval)registers.pc,
		 (uval)registers.lr);
      GDBIO::ClassInit();
      GDBIO::GDBWrite("-", 1);
  }

  while (1) {
      error = 0;
      outBuffer = putPacketStart();
      inBuffer = getPacket();
      switch (inBuffer[0]) {
      case '?' :
	  outBuffer[0] = 'S';
	  outBuffer[1] =  hexchars[sigval >> 4];
	  outBuffer[2] =  hexchars[sigval % 16];
	  outBuffer[3] =  '\0';
	  break;
      case 'd' :
	  remote_debug = !(remote_debug);  /* toggle debug flag */
	  break;
      case 'g' : /* return the value of the CPU registers */
	  mem2hex((char*) &registers, outBuffer, NUMREGBYTES, 0);
	  break;
      case 'G' : /* set the value of the CPU registers - return OK */
	  hex2mem(&inBuffer[1], (char*) &registers, NUMREGBYTES, 0);
	  strcpy(outBuffer,"OK");
	  break;

	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
      case 'm' :
	  /* TRY TO READ %lx,%lx.  IF SUCCEED, SET PTR = 0 */
	  ptr = &inBuffer[1];
	  if (hexToInt(&ptr,&addr))
	      if (*(ptr++) == ',')
		  if (hexToInt(&ptr,&length))
		      {
			  ptr = 0;
			  mem_err = 0;
			  mem2hex((char*) addr, outBuffer, length, 1);
			  if (mem_err) {
			      strcpy (outBuffer, "E03");
			      debug_error ("memory fault", NULL);
			  }
		      }

	  if (ptr)
	      {
		  strcpy(outBuffer,"E01");
		  debug_error("malformed read memory command: %s",inBuffer);
	      }
	  break;

	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
      case 'M' :
	  /* TRY TO READ '%lx,%lx:'.  IF SUCCEED, SET PTR = 0 */
	  ptr = &inBuffer[1];
	  if (hexToInt(&ptr,&addr))
	      if (*(ptr++) == ',')
		  if (hexToInt(&ptr,&length))
		      if (*(ptr++) == ':')
			  {
			      mem_err = 0;
			      hex2mem(ptr, (char*) addr, length, 1);

			      if (mem_err) {
				  strcpy (outBuffer, "E03");
				  debug_error ("memory fault", NULL);
			      } else {
				  strcpy(outBuffer,"OK");
			      }

			      ptr = 0;
			  }
	  if (ptr)
	      {
		  strcpy(outBuffer,"E02");
		  debug_error("malformed write memory command: %s",inBuffer);
	      }
	  break;

	  /* CSSS;AA..AA    Continue at address AA..AA(optional) */
	  /* cAA..AA    Continue at address AA..AA(optional) */
      case 'c' :
      case 'C':
	  ptr = &inBuffer[1];
	  if (inBuffer[0] == 'C') {
	      /* just hope that ';' is in first 8  (it may not be there) */
	      /* there's no way to know how long this buffer is */
	      i = 0;
	      while (i < 8) {
		  if (*ptr == ';') break;
		  ++i;
		  ++ptr;
	      }

	      if (i < 8 && hexToInt(&ptr, &addr)) {
		  registers.pc = addr;
	      }
	  } else {
	      /* try to read optional parameter, pc unchanged if no parm */
	      if (hexToInt(&ptr,&addr))
		  registers.pc = addr;
	  }

          newPC = registers.pc;

          /* clear the trace bit */
          registers.ps &= ~PSL_SE;

          /* set the trace bit if we're stepping */
          if (inBuffer[0] == 's') registers.ps |= PSL_SE;

          /*
           * If we found a match for the PC AND we are not returning
           * as a result of a breakpoint (33),
           * trace exception (9), nmi (31), jmp to
           * the old exception handler as if this code never ran.
           */
	  return;

          break;

      case 'D' :
	  // Detach and let the program continue to execute
	  strcpy(outBuffer,"OK");
	  /* reply to the request */
	  putPacketFinish();
	  GDBIO::GDBClose();
	  return;
	  break;

	  /* kill the program */
      case 'k' :  // We'll take 'kill' to mean 'reboot'
	  // just kill the K42 Process.
	  GDBIO::GDBClose();
	  DREFGOBJ(TheProcessRef)->kill();
	  /* NOTREACHED */

      case 's':
      case 'e':
      default:
//	  err_printf("Unsupported gdb command: %c [0x%x]\n",
//		     inBuffer[0],(int)inBuffer[0]);
	  /* received a command we don't recognize --
	     send empty response per gdb spec */
	  strcpy(outBuffer,"");
	  //FIXME after new thread model
	  break;				// not implemented yet

    } /* switch */
    /* reply to the request */
    putPacketFinish();
    }
}

SLock GDBLock;	// FIXME: relying on zeroed BSS being proper initial value

/*
 * Returns 0 if trap has been handled at this level, 1 otherwise.
 */
uval GDBStubPrelude(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		    VolatileState *vsp, NonvolatileState *nvsp)
{
    if (InDebugger()) {
	if (gdb_mem_fault_possible) {
	    /* skip the failing instruction and set mem_err */
	    vsp->iar += 4;
	    mem_err = 1;
	    return 0;	// caller should resume from this trap
	}

	uval callChain[20];
	GetCallChainSelf(0, callChain, 20);
	err_printf("Trap in Debugger at pc %llx, lr %llx\n",
		   vsp->iar, vsp->lr);
	err_printf("    Call chain:\n");
	for (uval i = 0; (i < 20) && (callChain[i] != 0); i++) {
	    err_printf("        0x%lx\n", callChain[i]);
	}
	err_printf("    Info for original trap:\n");
	err_printf("        pc:          %lx\n", gdb_trapPC);
	err_printf("        lr:          %lx\n", gdb_trapLR);
	err_printf("        sp:          %lx\n", gdb_trapSP);
	err_printf("        vector:      %lx\n", gdb_vector);
	err_printf("        trapInfo:    %lx\n", gdb_trapInfo);
	err_printf("        trapAuxInfo: %lx\n", gdb_trapAuxInfo);
	err_printf("        Call chain:\n");
	GetCallChain(gdb_trapSP, callChain, 20);
	for (uval i = 0; (i < 20) && (callChain[i] != 0); i++) {
	    err_printf("            0x%lx\n", callChain[i]);
	}
	while (1);	// loop forever
    }

    gdb_trapPC = vsp->iar;
    gdb_trapLR = vsp->lr;
    gdb_trapSP = vsp->r1;
    gdb_vector = trapNumber;
    gdb_trapInfo = trapInfo;
    gdb_trapAuxInfo = trapAuxInfo;

    // use aux stack in case of page fault in debugger
    EnterDebugger();

    GDBLock.acquire();

    memcpy(&registers.gpr[0],  &vsp->r0,   (13 - 0  + 1)*sizeof(uval64));
    memcpy(&registers.gpr[14], &nvsp->r14, (31 - 14 + 1)*sizeof(uval64));
    memcpy(&registers.fpr[0],  &vsp->f0,   (13 - 0  + 1)*sizeof(uval64));
    memcpy(&registers.fpr[14], &nvsp->f14, (31 - 14 + 1)*sizeof(uval64));
    registers.pc = vsp->iar;
    registers.ps = vsp->msr ;
    registers.cr = (uval32) vsp->cr;
    registers.lr = vsp->lr;
    registers.ctr = vsp->ctr;
    registers.xer = (uval32) vsp->xer;
    registers.fpscr = (uval32) vsp->fpscr;

    SavedExtRegs = extRegsLocal;
    RESET_PPC();
    SavedPrimitivePPCFlag = Scheduler::SetAllowPrimitivePPC(1);

    return 1;
}

void GDBStubPostlude(VolatileState *vsp, NonvolatileState *nvsp)
{
    Scheduler::SetAllowPrimitivePPC(SavedPrimitivePPCFlag);
    extRegsLocal = SavedExtRegs;

    memcpy(&vsp->r0,  &registers.gpr[0],   (13 - 0  + 1)*sizeof(uval64));
    memcpy(&nvsp->r14, &registers.gpr[14], (31 - 14 + 1)*sizeof(uval64));
    memcpy(&vsp->f0,  &registers.fpr[0],   (13 - 0  + 1)*sizeof(uval64));
    memcpy(&nvsp->f14, &registers.fpr[14], (31 - 14 + 1)*sizeof(uval64));

    vsp->iar = registers.pc;
    vsp->msr = registers.ps;
    vsp->cr = (uval32) registers.cr;
    vsp->lr = registers.lr;
    vsp->ctr = registers.ctr;
    vsp->xer = (uval64) registers.xer;
    vsp->fpscr = (uval64) registers.fpscr;

    GDBLock.release();

    // reestablish main stack for next trap
    ExitDebugger();
}

extern "C" void
GDBStub_UserTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		 NonvolatileState *nvsp)
{
    VolatileState *vsp = extRegsLocal.dispatcher->trapStatePtr();

    if (!GDBStubPrelude(trapNumber, trapInfo, trapAuxInfo, vsp, nvsp)) {
	return;		// trap has been handled
    }

    uval disabledSave = extRegsLocal.dispatcher->trapDisabledSave;

    handle_exception(gdb_vector);

    extRegsLocal.dispatcher->trapDisabledSave = disabledSave;

    // trapStatePtr() may have changed
    vsp = extRegsLocal.dispatcher->trapStatePtr();

    GDBStubPostlude(vsp, nvsp);
}

extern "C" void
GDBStub_KernelTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		   VolatileState *vsp, NonvolatileState *nvsp)
{
    if (!GDBStubPrelude(trapNumber, trapInfo, trapAuxInfo, vsp, nvsp)) {
	return;		// trap has been handled
    }

    uval disabledSave = Scheduler::IsDisabled();
    if (!disabledSave) Scheduler::Disable();

    handle_exception(gdb_vector);

    if (!disabledSave) Scheduler::Enable();

    GDBStubPostlude(vsp, nvsp);
}

extern "C" void
GdbExit(sval sigval)
{
    char * outBuffer = 0;

    if (GDBIO::IsConnected()) {
	/* reply to host that we're terminated */
	outBuffer = putPacketStart();
	outBuffer[0] = 'X';
	outBuffer[1] = hexchars[sigval >> 4];
	outBuffer[2] = hexchars[sigval % 16];
	outBuffer[3] = '\0';
	putPacketFinish();
	GDBIO::GDBClose();
    }
}
