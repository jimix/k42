#ifndef __KPARMS_H
#define __KPARMS_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KParms.H,v 1.3 2004/10/26 06:06:59 cyeoh Exp $
 *****************************************************************************/

class KParms
{
public:
    KParms(void *data, uval maxDataSize);
    /* Format of input block is:
       (uval) Number of Parts
       (uval) Size of Part
              Data
       (uval) Size of Part
       ....
    */

    ~KParms();

    DEFINE_GLOBALPADDED_NEW(KParms);

    uval32 getNumParts() const;

    const void *getData(uval32 part) const;
    /* part is indexed from 0 */
    /* Return NULL if part does not exist */
    /* First 4 bytes of buffer is uval32 which specifies
       the size of the buffer */

    void addPart(uval length);

    int updatePart(uval32 part, const void *data, uval32 length);
    /* Returns 0 on success, non-zero otherwise */

    static void ClassInit(void *data, uval maxDataSize);

    static KParms *TheKParms;

private:
    char *bootData; /* Block of boot data */
    uval32 numParts;
    uval32 *offsets;
    uval maxBootDataSize;
};

#endif
