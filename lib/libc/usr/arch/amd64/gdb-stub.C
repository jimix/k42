/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: gdb-stub.C,v 1.12 2004/04/06 21:00:35 rosnbrg Exp $
 *****************************************************************************/

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

// VVVVVVVVVVVVVVVVVV fixme just x86 yet XXX pdb

#include "sys/sysIncs.H"
#include "GDBIO.H"
#include <scheduler/DispatcherDefaultExp.H>
#include <sync/SLock.H>

#ifdef notyetpdb	// not needed with simics
#include <sys/thinwire.H>
#endif /* #ifdef notyetpdb	// not needed with ... */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

#ifdef notyetpdb	// hopefully not needed simics
static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];
static short error;
static ExtRegs SavedExtRegs;

static char connected_kernel = 0; // boolean flag. 0 means not connected
static char connected_user = 0; // boolean flag. 0 means not connected

static sval remote_debug = 0;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */
#endif /* #ifdef notyetpdb	// hopefully not ... */


static const char hexchars[]="0123456789abcdef";

/* Number of bytes of registers.  */
#define NUMREGBYTES 64
enum regnames {RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
	       R8, R9, R10, R11, R12, R13, R14, R15,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};


#ifdef notyetpdb	// hopefully not needed simics
/*
 * these should not be static cuz they can be used outside this module
 */
static uval registers[NUMREGBYTES/4];
#endif /* #ifdef notyetpdb	// hopefully not ... */


#ifdef notyetpdb	// hopefully not needed simics
static sval
hex(char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}
#endif /* #ifdef notyetpdb	// hopefully not ... */

#ifdef notyetpdb	// hopefully not needed simics
static uval thinWireChannel=GDB_USER_CHANNEL;
static char *
getPacket(void)
{
    unsigned char checksum;
    unsigned char xmitcsum;
    sval i;
    sval count;

    for (;;) {
	count = thinwireRead(thinWireChannel, remcomInBuffer, BUFMAX-2);
	if (count == 0) {
	    /*
	     * thinwireRead() has a window during which it's a BAD IDEA to
	     * interrupt execution and save the state of the computation.  To
	     * lessen the probability of hitting that window, we waste some
	     * time here.
	     */
	    for (i = 0; i < 100; i++) strlen("");
	    continue;
	}

	if(remote_debug) {
	    remcomInBuffer[count] = 0;
	    cprintf("G%ld %s\n",count,remcomInBuffer);
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
		thinwireWrite(thinWireChannel, "+", 1);
		if ((count >= 7) && (remcomInBuffer[3] == ':')) {
		    thinwireWrite(thinWireChannel, remcomInBuffer+1, 2);
		    return (remcomInBuffer + 4);
		} else {
		    return (remcomInBuffer + 1);
		}
	    }
	}

	thinwireWrite(thinWireChannel, "-", 1);
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

    if(remote_debug) {
	remcomOutBuffer[count+3] = 0;
	cprintf("P%ld %s\n",count+3,remcomOutBuffer);
    }

    do {
	thinwireWrite(thinWireChannel, remcomOutBuffer, count + 3);
	while (thinwireRead(thinWireChannel, &ch, 1) < 1) /* no-op */;
    } while (ch != '+');
}

static void
debug_error(char * format, char * parm)
{
  if (remote_debug) cprintf (format,parm);
}

/* flag set if a mem fault is possible.  Faults are handled by
 * skipping the forcing a return from the faulting routine and setting
 * mem_err to 1
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
      return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char*
hex2mem(char* buf, char* mem, sval count, sval may_fault)
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
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);
          set_char(mem, ch);
	  if (may_fault && mem_err) break;
          mem++;
      }
      if (may_fault) gdb_mem_fault_possible = 0;
      return(mem);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
static sval
computeSignal(sval exceptionVector)
{
  sval sigval;
  switch (exceptionVector) {
    case 0 : sigval = 8; break; /* divide by zero */
    case 1 : sigval = 5; break; /* debug exception */
    case 3 : sigval = 5; break; /* breakpoint */
    case 4 : sigval = 16; break; /* into instruction (overflow) */
    case 5 : sigval = 16; break; /* bound instruction */
    case 6 : sigval = 4; break; /* Invalid opcode */
    case 7 : sigval = 8; break; /* coprocessor not available */
    case 8 : sigval = 7; break; /* double fault */
    case 9 : sigval = 11; break; /* coprocessor segment overrun */
    case 10 : sigval = 11; break; /* Invalid TSS */
    case 11 : sigval = 11; break; /* Segment not present */
    case 12 : sigval = 11; break; /* stack exception */
    case 13 : sigval = 11; break; /* general protection */
    case 14 : sigval = 11; break; /* page fault */
    case 16 : sigval = 7; break; /* coprocessor error */
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

/* Put the error code here just in case the user cares.  */
static sval gdb_x86errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
static sval gdb_vector = -1;

/*
 * This function does all command procesing for interfacing to gdb.
 */
static void
handle_exception(sval exceptionVector)
{
  sval   sigval;
  sval   addr, length;
  char * ptr;
  sval   newPC;
  char * inBuffer;
  char * outBuffer;

  if (remote_debug) cprintf("vector=0x%lx, sr=0x%lx, pc=0x%lx\n",
			    exceptionVector,
			    registers[ PS ],
			    registers[ PC ]);
  sigval = computeSignal(exceptionVector);

  uval connected = 1;
  if ((thinWireChannel==GDB_CHANNEL)&&!connected_kernel) {
      connected = 0;			// not connected yet
      connected_kernel = 1;		// will be in a bit
  } else if ((thinWireChannel==GDB_USER_CHANNEL)&&!connected_user) {
      connected = 0;			// not connected yet
      connected_user = 1;		// will be in a bit
  }

  if (!connected) {
#if 1 /* locked gdb */
      static BLock lock;			// only allow one thread through
      err_printf("acquiring debugging lock\n");
      lock.acquire();
#endif /* #if 1 */
      err_printf("Connecting to %s GDB via thinwire channel pc %lx\n",
		 (thinWireChannel == GDB_CHANNEL)?"Kernel":"User",
		 registers[PC]);
      thinwireWrite(thinWireChannel, "-", 1);
  } else {
    /* reply to host that an exception has occurred */
    outBuffer = putPacketStart();
    outBuffer[0] = 'S';
    outBuffer[1] =  hexchars[sigval >> 4];
    outBuffer[2] =  hexchars[sigval % 16];
    outBuffer[3] =  '\0';
    putPacketFinish();
  }

  while (1==1) {
    error = 0;
    outBuffer = putPacketStart();
    inBuffer = getPacket();
    switch (inBuffer[0]) {
      case '?' :   outBuffer[0] = 'S';
                   outBuffer[1] =  hexchars[sigval >> 4];
                   outBuffer[2] =  hexchars[sigval % 16];
                   outBuffer[3] =  '\0';
                 break;
      case 'd' : remote_debug = !(remote_debug);  /* toggle debug flag */
                 break;
      case 'g' : /* return the value of the CPU registers */
                mem2hex((char*) registers, outBuffer, NUMREGBYTES, 0);
                break;
      case 'G' : /* set the value of the CPU registers - return OK */
                hex2mem(&inBuffer[1], (char*) registers, NUMREGBYTES, 0);
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

     /* cAA..AA    Continue at address AA..AA(optional) */
     /* sAA..AA   Step one instruction from AA..AA(optional) */
     case 'c' :
     case 's' :
          /* try to read optional parameter, pc unchanged if no parm */
         ptr = &inBuffer[1];
         if (hexToInt(&ptr,&addr))
             registers[ PC ] = addr;

          newPC = registers[ PC];

          /* clear the trace bit */
          registers[ PS ] &= 0xfffffeff;

          /* set the trace bit if we're stepping */
          if (inBuffer[0] == 's') registers[ PS ] |= 0x100;

          /*
           * If we found a match for the PC AND we are not returning
           * as a result of a breakpoint (33),
           * trace exception (9), nmi (31), jmp to
           * the old exception handler as if this code never ran.
           */
	  return;

          break;

#if 0
      /* kill the program */
      case 'k' :  // We'll take 'kill' to mean 'reboot'
	bios_reboot();
	/* NOTREACHED */
#endif /* #if 0 */

      } /* switch */
    /* reply to the request */
    putPacketFinish();
    }
}
#endif /* #ifdef notyetpdb	// hopefully not ... */

SLock GDBLock;	// FIXME: relying on zeroed BSS being proper initial value

extern "C" void
GdbUserService(DispatcherDefaultExpRegs *erp)
{
#ifdef notyetpdb	// XXX, with simics hopefully no need yet
    gdb_vector = erp->Trap_trapNumber;
    gdb_x86errcode = erp->Trap_x86ErrorCode;
    VolatileState *psp = extRegsLocal.dispatcher->trapStatePtr();

    if (InDebugger()) {
	if (!gdb_mem_fault_possible) {
	    err_printf("Trap in Debugger\n");
	    while(1);	// loop forever
	}
	/* simulate a return from the routine that faulted, and set mem_err. */
	psp->faultFrame.rsp = psp->rbp;				// movl rbp,rsp
	psp->rbp = *((uval64 *) psp->faultFrame.rsp);		// popl rbp
	psp->faultFrame.rsp += sizeof(uval64);
	psp->faultFrame.rip =
	    (codeAddress) *((uval64 *) psp->faultFrame.rsp);	// ret
	psp->faultFrame.rsp += sizeof(uval64);
	mem_err = 1;
    } else {
	GDBLock.acquire();
	// use aux stack in case of page fault in debugger
	EnterDebugger();

	registers[RBX] = erp->rbx;
	registers[RSI] = erp->rsi;
	registers[RDI] = erp->rdi;
	registers[RAX] = psp->rax;
	registers[RCX] = psp->rcx;
	registers[RDX] = psp->rdx;
	registers[RBP] = psp->rbp;
	registers[R8] = psp->r8;
	registers[R9] = psp->r9;
	registers[R10] = psp->r10;
	registers[R11] = psp->r11;
//	registers[R12] = psp->r12;
//	registers[R13] = psp->r13;
//	registers[R14] = psp->r14;
//	registers[R15] = psp->r15;
	registers[RSP] = psp->faultFrame.rsp;
	registers[PC] = (uval) psp->faultFrame.rip;
	registers[PS] = psp->faultFrame.rflags;

	registers[CS] = psp->faultFrame.cs;
	registers[SS] = psp->faultFrame.ss;
	/*
	 * The data-segment registers aren't part of the TRAP_ENTRY interface.
	 * We'll simply reflect the current values.
	 */
	uval16 segreg;
	asm("movw %%ds, %0" : "=ax" (segreg)); registers[DS] = segreg;
	asm("movw %%es, %0" : "=ax" (segreg)); registers[ES] = segreg;
	asm("movw %%fs, %0" : "=ax" (segreg)); registers[FS] = segreg;
	asm("movw %%gs, %0" : "=ax" (segreg)); registers[GS] = segreg;

	SavedExtRegs = extRegsLocal;
	RESET_PPC();

	uval disabledSave = statePageLocal.trapDisabledSave;

	handle_exception(gdb_vector);

	statePageLocal.trapDisabledSave = disabledSave;

	extRegsLocal = SavedExtRegs;

	erp->rbx = registers[RBX];
	erp->rsi = registers[RSI];
	erp->rdi = registers[RDI];
	psp->rax = registers[RAX];
	psp->rcx = registers[RCX];
	psp->rdx = registers[RDX];
	psp->rbp = registers[RBP];
	psp->r8 = registers[R8];
	psp->r9 = registers[R9];
	psp->r10 = registers[R10];
	psp->r11 = registers[R11];
//	psp->r12 = registers[R12];
//	psp->r13 = registers[R13];
//	psp->r14 = registers[R14];
//	psp->r15 = registers[R15];
	psp->faultFrame.rsp = registers[RSP];
	psp->faultFrame.rip = (codeAddress) registers[PC];
	psp->faultFrame.rflags = registers[PS];
	psp->faultFrame.cs = registers[CS];
	psp->faultFrame.ss = registers[SS];
	/*
	 * We don't try to restore data-segment registers.
	 */

	// reestablish main stack for next trap
	ExitDebugger();
	GDBLock.release();
    }
#endif /* #ifdef notyetpdb	// XXX, with simics ... */
}

struct GdbFrame {
    uval64 trapNumber;
    uval64 x86ErrorCode;
    uval64 rsp, rdi, rsi, rbx;
    DispatcherDefaultExpRegs ps;
};

/* derived from Linux x86-64
 */

#define rdmsrl(msr,val) do { unsigned long a__,b__;			\
       __asm__ __volatile__("rdmsr" 					\
           : "=a" (a__), "=d" (b__)	 				\
           : "c" (msr)); val = a__ | (b__<<32); } while(0);

extern "C" void
//GdbKernelService(GdbFrame *gfp)
GdbKernelService()
{
        unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs;
        unsigned int fsindex,gsindex;
        unsigned int ds,cs;

        asm("movl %%ds,%0" : "=r" (ds));
        asm("movl %%cs,%0" : "=r" (cs));
        asm("movl %%fs,%0" : "=r" (fsindex));
        asm("movl %%gs,%0" : "=r" (gsindex));

        rdmsrl(0xc0000100, fs);
        rdmsrl(0xc0000101, gs);
        err_printf("FS: %016lx(%04x) GS: %016lx(%04x) DS: %04x CS: %04x\n",
               fs,fsindex,gs,gsindex,ds,cs);

        asm("movq %%cr0, %0": "=r" (cr0));
        asm("movq %%cr2, %0": "=r" (cr2));
        asm("movq %%cr3, %0": "=r" (cr3));
        asm("movq %%cr4, %0": "=r" (cr4));

        err_printf("CR0: %016lx CR2: %016lx CR3: %016lx CR4: %016lx\n",
               cr0, cr2, cr3, cr4);
#ifdef notyetpdb	// XXX, with simics hopefully no need yet
    gdb_vector = gfp->trapNumber;
    gdb_x86errcode = gfp->x86ErrorCode;

    if (InDebugger()) {
	if (!gdb_mem_fault_possible) {
	    err_printf("Trap in Debugger\n");
	    while(1);	// loop forever
	}
	/* simulate a return from the routine that faulted, and set mem_err. */
// XXX
	gfp->rsp = gfp->ps.rbp;					// movl ebp,esp
	gfp->ps.rbp = *((uval64 *) gfp->rsp);			// popl ebp
	gfp->rsp += sizeof(uval64);
	gfp->ps.faultFrame.rip =
	    (codeAddress) *((uval64 *) gfp->rsp);		// ret
	gfp->rsp += sizeof(uval64);
	mem_err = 1;
// XXX
    } else {
	GDBLock.acquire();
	// use aux stack in case of page fault in debugger
	EnterDebugger();

	registers[RSP] = gfp->rsp;
	registers[RDI] = gfp->rdi;
	registers[RSI] = gfp->rsi;
	registers[RBX] = gfp->rbx;
	registers[RAX] = gfp->ps.rax;
	registers[RCX] = gfp->ps.rcx;
	registers[RDX] = gfp->ps.rdx;
	registers[RBP] = gfp->ps.rbp;
// more XXX
	registers[PC] = (uval) gfp->ps.faultFrame.rip;
	registers[CS] = gfp->ps.faultFrame.cs;
	registers[PS] = gfp->ps.faultFrame.rflags;
	/*
	 * The stack- and data-segment registers aren't part of the GdbFrame
	 * interface.  We'll simply reflect the current values.
	 */
	uval16 segreg;
	asm("movw %%ss, %0" : "=ax" (segreg)); registers[SS] = segreg;
	asm("movw %%ds, %0" : "=ax" (segreg)); registers[DS] = segreg;
	asm("movw %%es, %0" : "=ax" (segreg)); registers[ES] = segreg;
	asm("movw %%fs, %0" : "=ax" (segreg)); registers[FS] = segreg;
	asm("movw %%gs, %0" : "=ax" (segreg)); registers[GS] = segreg;

	uval saveOldChannel = thinWireChannel;
	thinWireChannel = GDB_CHANNEL;

	handle_exception(gdb_vector);

	thinWireChannel = saveOldChannel;

	gfp->rsp = registers[RSP];
	gfp->rdi = registers[RDI];
	gfp->rsi = registers[RSI];
	gfp->rbx = registers[RBX];
	gfp->ps.rax = registers[RAX];
	gfp->ps.rcx = registers[RCX];
	gfp->ps.rdx = registers[RDX];
	gfp->ps.rbp = registers[RBP];
	gfp->ps.faultFrame.rip = (codeAddress) registers[PC];
	gfp->ps.faultFrame.cs = registers[CS];
	gfp->ps.faultFrame.rflags = registers[PS];
	/*
	 * We don't try to restore stack- and data-segment registers.
	 */

	// reestablish main stack for next trap
	ExitDebugger();
	GDBLock.release();
    }
#endif /* #ifdef notyetpdb	// XXX, with simics ... */
}
