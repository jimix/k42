#ifndef __BOOTPRINTF_H_
#define __BOOTPRINTF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BootPrintf.H,v 1.3 2005/03/15 02:35:10 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * class to buffer early printfs until console is up
 * **************************************************************************/

sval
vsprintf(char *str, const char *fmt, va_list ap);

class BootPrintf {
    static struct StaticStuff {
	char* buf;
	char* next;
    } staticStuff;
    enum {size = 4096, msg=256};
public:
    static void Init(MemoryMgrPrimitiveKern *memory) {
	uval bufaddr;
	memory->alloc(bufaddr, size);
	staticStuff.next = staticStuff.buf = (char*)bufaddr;
    }

    static void Printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	if (staticStuff.next - staticStuff.buf + msg > size) {
	    if(staticStuff.next - staticStuff.buf > 32) {
		strcpy(staticStuff.next, "BootPrintf buffer full\n");
		staticStuff.next += strlen(staticStuff.next) + 1;
	    }
	    return;
	}
	sval len;
	len = vsprintf(staticStuff.next, fmt, ap);
        va_end(ap);
	staticStuff.next += len+1;
    }

    static void VPrintf(const char* fmt, va_list ap) {
	if (staticStuff.next - staticStuff.buf + msg > size) {
	    if(staticStuff.next - staticStuff.buf > 32) {
		strcpy(staticStuff.next, "BootPrintf buffer full\n");
		staticStuff.next += strlen(staticStuff.next) + 1;
	    }
	    return;
	}
	sval len;
	len = vsprintf(staticStuff.next, fmt, ap);
	staticStuff.next += len+1;
    }

    static void PrintAll(MemoryMgrPrimitiveKern *memory) {
	char* p = staticStuff.buf;	// for debugging, use a temp variable
	while(p < staticStuff.next) {
	    err_printf("%s", p);
	    p += strlen(p) + 1;
	}
	memory->rememberChunk(
	    uval(staticStuff.buf), uval(staticStuff.buf)+size);
    }
};

#endif /* #ifndef __BOOTPRINTF_H_ */
