/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simos.C,v 1.30 2005/02/09 18:45:43 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: services built on top of simSupport.
 * **************************************************************************/

#include <kernIncs.H>
#include "simos.H"
#include <misc/hardware.H>
#include <sys/ComPort.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <sys/IOChan.H>

uval
bStrLen (char *s)
{
    uval i = 0;
    while (*s++) i++;
    return i;
}

void
bStrCat (char *s, char *t)
{
    uval ls = bStrLen (s);
    char *r = s + ls;
    while ((*r++ = *t++));
}

void
bStrCpy(char *s, char *t)
{
    while ((*s++ = *t++));
}

char _locHex[] = "0123456789ABCDEF";
char *
printHex (char *s, uval v)
{
    uval i, j;

    for (i=0; i<8; i++) {
	j = (v >> (28 - (4 * i))) & 0xF;
	*s++ = _locHex[j];
    }
    *s++=0;
    return s;
}

// bprintf allows printing from the very first kernel instruction far
// before err_printf is ready, while not in the code now they have been
// left here in case they are needed
void
bprintf(char *buf)
{
    uval len;
    char lbuf[256];

    bStrCpy(lbuf,buf);
    bStrCat (lbuf, "\r\n");
    len = bStrLen(buf);
    // a kludge to determine whether we're in virtual mode assumes
    // we're called from the kernel
    if (((uval)lbuf) > (0x10000000)) {
	SimOSSupport(SIMOS_CONS_FUN, lbuf, len, VIRT_MODE_SIMOS_CALL);
    } else {
	SimOSSupport(SIMOS_CONS_FUN, lbuf, len, REAL_MODE_SIMOS_CALL);
    }

}

// b1printf allow printing from the very first kernel instruction far
// before err_printf is ready, while not in the code now they have been
// left here in case they are needed
void
b1printf(char *buf, uval val)
{
    char *bufp;
    char lbuf[256];

    bStrCpy(lbuf,buf);

    bufp = lbuf + bStrLen(lbuf);
    bufp = printHex (bufp, val);
    bStrCat (bufp, "\r\n");
    bprintf (lbuf);
}

/*
 * Asking simos to read/write to/from virtual addresses is unreliable because
 * virtual address mappings can be evicted from the hashtable at any time.
 * We therefore do all ThinIP I/O through a buffer whose virtual and physical
 * addresses are known.
 */
#define SIMOS_THIN_IP_BUF_SIZE 256
static uval simosThinIPBufVirt;
static uval simosThinIPBufPhys;

void
simosThinIPInit(MemoryMgrPrimitiveKern *memory)
{
    memory->alloc(simosThinIPBufVirt, SIMOS_THIN_IP_BUF_SIZE, 8);
    // Larger alignment not needed because primitive allocations are
    // contiguous in physical as well as in virtual memory.
    simosThinIPBufPhys = memory->physFromVirt(simosThinIPBufVirt);
}

sval
simosThinIPWrite(const char *buf, uval length)
{
    uval origLength = length;

    while (length > 0) {
	sval rc;
	uval amt = MIN(length, SIMOS_THIN_IP_BUF_SIZE);
	memcpy((void *) simosThinIPBufVirt, buf, amt);
	rc = SimOSSupport(SimThinIPWriteK, simosThinIPBufPhys, amt);
	if (rc < 0) return rc;
	length -= amt;
	buf += amt;
    }
    return origLength;
}


sval
simosThinIPRead(char *buf, uval length)
{
    sval rc;

    length = MIN(length, SIMOS_THIN_IP_BUF_SIZE);
    rc = SimOSSupport(SimThinIPReadK, simosThinIPBufPhys, length);
    if (rc > 0) {
	memcpy(buf, (void *) simosThinIPBufVirt, rc);
    }
    return rc;
}

sval
simosReadCharStdin()
{
    sval ch;

    ch = SimOSSupport(SimReadCharStdinK);
    return ch;
}

void
simosConsoleWrite(const char *buf, uval length)
{
    // a kludge to determine whether we're in virtual mode assumes
    // we're called from the kernel
    if (((uval)buf) > (0x10000000)) {
	// We have to be disabled to have some hope that buf's virtual mapping
	//    isn't evicted from the hashtable before we get to simos.  We
	//    touch each page of buf to increase the likelihood that it's
	//    mapped.  We can not cleanly convert to real mode because this
	//    code is called very early, before the generic interfaces for
	//    virtToReal are setup.
	InterruptState is;
	uval reenable = 0;
	if (hardwareInterruptsEnabled()) {
	    disableHardwareInterrupts(is);
	    reenable = 1;
	}
	tassertSilent(!hardwareInterruptsEnabled(),SIMOS_BREAKPOINT);
	for (const char *p = buf; p < buf + length; p += PAGE_SIZE) {
	    * (volatile char *) p;
	}
	SimOSSupport(SIMOS_CONS_FUN, buf, length, VIRT_MODE_SIMOS_CALL);

	if (reenable)
	    enableHardwareInterrupts(is);
    } else {
	SimOSSupport(SIMOS_CONS_FUN, buf, length, REAL_MODE_SIMOS_CALL);
    }
}

// FIXME where does this really belong
/* physmemcpy (destination source length) */
void *
physmemcpy(void * t, const void * s, size_t len)
{
    return (void *)SimOSSupport(SimPhysMemCopyK, t, s, len);
}

void
simosGetTimeOfDay(uval32 &secs, uval32 &usecs)
{
    uval64 fullTime;

    fullTime = SimOSSupport(SimGetTimeK);

    secs = fullTime>>32;
    usecs = fullTime&0x00000000ffffffff;
}

struct SimTWChan: public IOChan {
    DEFINE_PRIMITIVE_NEW(SimTWChan);
    SimTWChan(uval num) : IOChan(num) {};
    virtual uval write(const char* buf, uval len, uval block) {
	return simosThinIPWrite(buf, len);
    }
    virtual uval read(char* buf, uval len, uval block) {
	uval ret;
	do {
	    ret = simosThinIPRead(buf, len);
	} while ( ret == 0 && block == 1);
	return ret;
    }
    virtual uval isReadable() {
	return 0;
    }
};

struct SimConChan: public IOChan {
    SimConChan(uval num): IOChan(num) {};

    /* We could be asked to create these when only the primitive
     * allocator is present, so we have to have both types of
     * allocators */

    DEFINE_PRIMITIVE_NEW(SimConChan);
    DEFINE_GLOBAL_NEW(SimConChan);
    virtual uval isReadable() {
	return 0;
    }
    virtual uval read(char* buf, uval length, uval block) {
	if (!length) return 0;
	char c = simosReadCharStdin();
	if (c != (char)0xff) return 1;
	return 0;
    }
    virtual uval write(const char* buf, uval length, uval block) {
	// Due to problems with GUD (gdb under emacs) we strip \r
	uval i = 0;
	uval ret = length;

	while (i < length) {
	    uval64 uc;
	    if (buf[i] != '\r') {
		uc = ((uval64)buf[i]) << 56;
		simosConsoleWrite((const char*)&uc, 1);
	    }
	    ++i;
	}
	return ret;
    }
};


IOChan*
getSimTWChan(MemoryMgrPrimitive *mem)
{
    return new(mem) SimTWChan(0);
}

IOChan*
getSimConChan(MemoryMgrPrimitive *mem)
{
    return new(mem) SimConChan(0);
}

