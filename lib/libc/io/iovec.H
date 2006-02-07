#ifndef __IOVEC_H_
#define __IOVEC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: iovec.H,v 1.4 2002/04/09 00:52:49 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Utility functions for scatter/gather
 * **************************************************************************/
#include <sys/uio.h>

static inline uval min(uval a, uval b) { return (a<b)? a:b ;};

static inline uval
memcpy_fromiovec(char *buf, const struct iovec* vec,
		 uval iovcount, uval buflen, uval startOffset = 0)
{
    uval total = 0;
    while (iovcount && buflen) {
	uval bytes = min(vec->iov_len, buflen);
	if (total >= startOffset) {
	    memcpy(buf+total, vec->iov_base, bytes);
	} else if (total+bytes > startOffset) {
	    memcpy(buf+total,
		   ((char*)vec->iov_base) + (total - startOffset),
		   bytes - (total - startOffset));
	}
	total += bytes;
	buflen -= bytes;
	++vec;
	--iovcount;
    }
    return total;
}

static inline uval
memcpy_toiovec(struct iovec* vec, const char* buf,
	       uval iovcount, uval buflen, uval startOffset = 0)
{
    uval total = 0;
    while (iovcount && buflen) {
	uval bytes = min(vec->iov_len, buflen);
	if (total >=startOffset) {
	    memcpy(vec->iov_base, buf+total, bytes);
	} else if (total+bytes > startOffset) {
	    memcpy(((char*)vec->iov_base) + (total - startOffset),
		   buf+total, bytes - (total - startOffset));
	}
	total += bytes;
	buflen -= bytes;
	++vec;
	--iovcount;
    }
    return total;
}

static inline uval
vecLength(const struct iovec* vec, uval vecCount)
{
    uval bytes = 0;
    for (uval i = 0; i<vecCount; ++i) {
	bytes += vec[i].iov_len;
    }
    return bytes;
}

static inline struct iovec*
iovec_chop(struct iovec* vec, uval choplen, uval &iovecLen)
{
    uval skip = 0;
    uval total = 0;
    for (uval i = 0; i<iovecLen; ++i) {
	if (total + vec[i].iov_len < choplen) {
	    ++skip;
	} else if (total + vec[i].iov_len > choplen) {
	    vec[i].iov_len -= choplen - total;
	    vec[i].iov_base= (void*)(((uval)vec[i].iov_base)+ choplen - total);
	    break;
	}
	total += vec[i].iov_len;
    }
    iovecLen-=skip;
    return vec+skip;
}

static inline
uval pageSpan(const struct iovec* vec, uval veclen)
{
    uval numPages = 0;
    for (uval i = 0; i < veclen; ++i) {
	uval start = (uval)vec[i].iov_base;
	uval end = start + vec[i].iov_len;
	if (end!=start) {
	    ++numPages;
	}
	start= (start + PAGE_SIZE) & ~PAGE_MASK;
	end  &= ~PAGE_MASK;
	if (end >= start) {
	    ++numPages;
	    numPages += (end - start) >> LOG_PAGE_SIZE;
	}
    }
    return numPages;
}
#endif /* #ifndef __IOVEC_H_ */
