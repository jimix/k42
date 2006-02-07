/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HVChan.C,v 1.1 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: IO Channel, hypervisor implementation
 * **************************************************************************/

#include "kernIncs.H"
#include "HVChan.H"

#if 0
void ping(char c)
{
    char header[8];
    header[0] = '#';
    header[1] = ' ' + ((1 >> 12) & 0x3f);
    header[2] = ' ' + ((1 >>  6) & 0x3f);
    header[3] = ' ' + ((1 >>  0) & 0x3f);
    header[4] = ' ' + 0;
    header[5] = c;
    uval x = *(uval*)header;
    hcall_put_term_char(NULL, 0, 6, x, 0);
}
#endif
uval
HVChannel::isReadable() {
    if (buflen) return 1;

    uval vals[4] = {0,};
    uval rc = hcall_get_term_char(&vals[0], id);

    if (rc == 0 && vals[0] > 0) {
	buflen = vals[0];
	memcpy(buf, &vals[1], buflen);
    }
    return buflen > 0;
}

uval
HVChannel::read(char* buffer, uval length, uval block) {
    uval ret = 0;
    while (length && (isReadable() || block == 1)) {
	if (length <= buflen) {
	    memcpy(buffer, buf, length);
	    memcpy(&buf[0], &buf[length], buflen - length);
	    ret+=length;
	    buflen -= length;
	    return ret;
	} else if (buflen) {
	    memcpy(buffer, buf, buflen);
	    ret += buflen;
	    buffer += buflen;
	    length -= buflen;
	    buflen = 0;
	}
    }
    return ret;
}

uval
HVChannel::write(const char* buffer, uval length, uval block)
{
    uval bytes = 0;
    while (bytes < length) {
	sval rc;
	uval64 a[2];
	uval l = 16;
	if ((length - bytes) < 16) {
	    l = length - bytes;
	}
	memcpy(&a[0], buffer, l);
	do {
	    rc = hcall_put_term_char(NULL, id, l, a[0], a[1]);
	} while (rc != 0);
	buffer += l;
	bytes += l;
    }
    return bytes;
}

