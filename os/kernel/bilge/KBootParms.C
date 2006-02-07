/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KBootParms.C,v 1.8 2005/07/13 17:02:59 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Parses boot time kernel parameters
 * Ascii pairs of key/value
 * **************************************************************************/
#include "kernIncs.H"
#include <alloc/AllocPool.H>

#include "KBootParms.H"
#include <meta/MetaKBootParms.H>

#undef VERBOSEKPARM

KBootParms *KBootParms::TheKBootParms = NULL;

KBootParms::KBootParms(const void *bootData, uval32 partRef)
    : parameterData(NULL), parameterDataLength(0), dataRef(partRef)
{
    if (!updateParameters(bootData, false)) {
	passertMsg(false, "Passed invalid data\n");
    }
}

KBootParms::~KBootParms()
{
    paramLookup.destroy();
    freeGlobal(parameterData, parameterDataLength);
}

bool
KBootParms::isBootData(const void *data)
{
    const char *dataBlock = static_cast<const char *>(data);
    uval32 dataLength;
    dataLength = *(uval32 *)dataBlock;
    if (dataLength > strlen("KBootParms")+1+sizeof(dataLength))  {
	dataBlock += sizeof(uval32);
	if (strcmp("KBootParms", dataBlock) == 0) return true;
    }
    return false;
}

const char *
KBootParms::getParameterValue(const char *parameter)
{
    char *val;
    const char *key;

#ifdef VERBOSEKPARM
    err_printf("KBootParm: %s = ", parameter);
#endif // #ifdef VERBOSEKPARM

    // Ugly hack:
    // We do a linear search because hash lookups are
    // comparing the pointers rather than the value of the pointers
    // and there seems to be no way of hashing anything which can't
    // be implicitly converted to a uval
    paramLookup.getFirst(key, val);
    do {
	if (strcmp(parameter, key)==0) {
#ifdef VERBOSEKPARM
	    err_printf("'%s'\n", val);
#endif // #ifdef VERBOSEKPARM
	    return val;
	}
    } while (paramLookup.getNext(key, val));

#ifdef VERBOSEKPARM
    err_printf("(not set)\n");
#endif // #ifdef VERBOSEKPARM

    return NULL;
}

bool
KBootParms::updateParameters(const void *bootData, bool updateKParms)
{
    char *index;

    if (!isBootData(bootData)) return false;

    if (parameterData) {

	// Have existing data we need to clean up 
	freeGlobal(parameterData, parameterDataLength);

	uval moreToDelete, restart=0;
	const char *key;
	char *value;

	// Remove all the entries from the hash
	moreToDelete = paramLookup.removeNext(key, value, restart);
	while (moreToDelete) {
	    moreToDelete = paramLookup.removeNext(key, value, restart);
	    err_printf("Deleting %s\n", key);
	}
    }

    parameterDataLength = *(uval32 *)bootData;
    parameterData = (char *)allocGlobal(parameterDataLength);
    memcpy(parameterData, bootData, parameterDataLength);
    index = parameterData + sizeof(uval32);

    passertMsg(strcmp(index, "KBootParms")==0, "Passed invalid boot data");
    index += strlen("KBootParms")+1;

    char *separator;
    int total_parm_length;
    while (index-parameterData < parameterDataLength) {
	err_printf("Got boot parameter data: %s\n", index);
	total_parm_length = strlen(index)+1;
	separator = strstr(index, "=");
	passertMsg(separator, "Bad boot parameter data %s", index);
	paramLookup.add(index, separator+1);
	*separator = 0;
	index += total_parm_length;
    }

    err_printf("Processed all boot parameters\n");
    bool success = true;

    if (updateKParms) {
	err_printf("About to update KParms\n");
	if (KParms::TheKParms->updatePart(dataRef, bootData, 
					  parameterDataLength)) {
	    success = false;
	}
	err_printf("Updated KParms\n");
    }
    
    return success;
}

SysStatus
KBootParms::getAllParameters(char *dataBlock, uval blockLength,
			     uval *usedBufSize)
{
    *usedBufSize = parameterDataLength - sizeof(uval32)
	- strlen("KBootParms") - 1;

    /* Check we have enough room to put the data into */
    if (*usedBufSize>blockLength) {
	err_printf("Need %lu bytes for buffer, only have %lu\n",
		   *usedBufSize, blockLength);
	return -1;
    }

    memcpy(dataBlock, parameterData+sizeof(uval32)+strlen("KBootParms")+1, 
	   *usedBufSize);

    return 0;
}

void
KBootParms::ClassInit(const KParms &Parameters)
{
    bool gotBootParms = false;

    MetaKBootParms::init();

    for (uval32 i=0; i<Parameters.getNumParts(); i++) {
	if (isBootData(Parameters.getData(i))) {
	    TheKBootParms = new KBootParms(Parameters.getData(i), i);
	    err_printf("Got KBootParms\n");
	    gotBootParms = true;
	    break;
	}
    }
    
    passertMsg(gotBootParms, "Failed to get boot parameters\n");
}

/* static */ SysStatus
KBootParms::_GetParameterValue(__inbuf(*) const char *parameter,
			       __outbuf(*:buflen) char *buf,
			       __in uval buflen)
{
    const char *val;
    val = TheKBootParms->getParameterValue(parameter);
    if (val) {
	strncpy(buf, val, buflen);
	if (strlen(val)>buflen-1) {
	    buf[buflen-1] = 0;
	    return 1;
	} else {
	    return 0;
	}
    }
    buf[0] = 0;
    return -1;
}

/* static */ SysStatus
KBootParms::_UpdateParameters(__in uval kaddr)
{
   if (TheKBootParms->updateParameters((char *)kaddr, true)) {
	return 0;
    } else {
	return -1;
    }
}

/* static */ SysStatus
KBootParms::_GetAllParameters(__in uval kaddr,
			      __in uval buflen,
			      __out uval *usedBufSize)
{
    return TheKBootParms->getAllParameters((char *)kaddr, buflen, 
					   usedBufSize);
}

