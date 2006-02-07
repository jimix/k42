#ifndef __KBOOTPARMS_H
#define __KBOOTPARMS_H

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KBootParms.H,v 1.4 2004/11/11 02:06:45 butrico Exp $
 *****************************************************************************/

#include <misc/HashSimple.H>
#include "KParms.H"

class KBootParms : public Obj {

#ifndef _IN_STUBGEN
public:
    KBootParms(const void *bootData, uval32 partRef);

    ~KBootParms();

    DEFINE_GLOBALPADDED_NEW(KBootParms);

    static bool isBootData(const void *data);

    static void ClassInit(const KParms &Parameters);

    const char *getParameterValue(const char *parameter);
    /* Returns NULL if parameter does not exist */

private:

    bool updateParameters(const void *bootData, bool updateKParms);
    
    SysStatus getAllParameters(char *dataBlock, uval blockLength,
			       uval *usedBufSize);

    char *parameterData;
    uval32 parameterDataLength;
    uval32 dataRef;

    HashSimple<const char *, char *, AllocGlobal, 4> paramLookup;

    static KBootParms *TheKBootParms;

#endif

__xpublic:
    static SysStatus _GetParameterValue(__inbuf(*) const char *parameter,
					__outbuf(*:buflen) char *value,
					__in uval buflen);

    static SysStatus _UpdateParameters(__in uval kaddr);

    static SysStatus _GetAllParameters(__in uval kaddr,
				       __in uval buflen,
 				       __out uval *usedBufSize);
    /* A block of data containing all the parameters is passed
       back. The first sizeof(uval32) bytes is the length of the buffer
       used (including the header). The paramter values follow in 
       null terminated char * string pairs. */

};


#endif /* __KBOOTPARMS_H */
