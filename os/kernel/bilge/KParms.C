/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KParms.C,v 1.3 2004/10/26 06:06:59 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: High level parsing and storage of
 * data passed to kernel at boot time. Just stores
 * anonymous blocks of data
 * **************************************************************************/
#include "kernIncs.H"
#include <alloc/AllocPool.H>
#include "KParms.H"

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#define __BIG_ENDIAN__   // defined BIG_ENDIAN by default if neither is defined
#endif /* #if !defined(__BIG_ENDIAN__) && ... */
#if defined(__BIG_ENDIAN__)
#define NTOHL(x) (x)
#elif defined(__LITTLE_ENDIAN__)
#define NTOHL(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */

KParms *KParms::TheKParms = NULL;

KParms::KParms(void *data, uval maxDataSize)
    : bootData(static_cast<char *>(data)), maxBootDataSize(maxDataSize)
{
    uval32 index;
    numParts = *(uval32 *)bootData;
    numParts = NTOHL(numParts);
    passertMsg(numParts>0, "Invalid data passed to KParms");

    err_printf("Found %i parts\n", numParts);

    offsets = (uval32 *) allocGlobal(sizeof(uval32)*numParts);
    index = sizeof(uval32);

    for (uval32 i=0; i<numParts; i++) {
	offsets[i] = index;
	index += *(uval32 *)(bootData + offsets[i]);
	err_printf("Part %u is of size %u. Offset is %u\n", i,
		   *(uval32 *)(bootData + offsets[i]), offsets[i]);
    }
}

KParms::~KParms()
{
    freeGlobal(offsets, sizeof(uval32)*numParts);
}

uval32
KParms::getNumParts() const
{
    return numParts;
}

const void *
KParms::getData(uval32 part) const
{
    if (part>=numParts) {
	return NULL;
    } else {
	return static_cast<void *>(bootData + offsets[part]);
    }
}

int
KParms::updatePart(uval32 part, const void *data, uval32 length)
{
    if (part>=numParts) {
	return -1;
    }

    passertMsg(*((uval32*)data)==length, 
	       "Passed bad data. Length does not match implicit length");

    /* Check we can fit the new data block in */
    uval32 currentPartSize = *((uval32 *)bootData+offsets[part]);
    uval32 end = *((uval32 *)bootData+offsets[numParts-1]);
    if (end + length - currentPartSize > maxBootDataSize) {
	return -2;
    }

    sval offset = length - currentPartSize;

    if (offset != 0 && part != numParts-1 ) {
	memmove(bootData + offsets[part+1] + offset, 
		bootData + offsets[part+1],
		end-offsets[part+1]);
    }
    
    /* Copy data in */
    memcpy(bootData + offsets[part], data, length);

    /* Fix up offsets */
    uval32 index = sizeof(uval32);
    for (uval32 i=0; i<numParts; i++) {
	offsets[i] = index;
	index += *(uval32 *)(bootData + offsets[i]);
	err_printf("Part %u is of size %u. Offset is %u\n", i,
		   *(uval32 *)(bootData + offsets[i]), offsets[i]);
    }

    return 0;
}

void
KParms::ClassInit(void *data, uval maxBootDataSize)
{
    TheKParms = new KParms(data, maxBootDataSize);
}
