#ifndef __FCMDEFAULTFILE_H_
#define __FCMDEFAULTFILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMFile.H,v 1.2 2005/06/15 04:49:13 jk Exp $
 *****************************************************************************/
/**
 * @class FCMFile
 * FCM that is backed by a real file.  Has to have a real
 * implementation of fsync...
 */

#include "mem/FCMDefault.H"
class FCMFile;
typedef FCMFile** FCMFileRef;

class FCMFile : public FCMDefault {
    
public:
    static SysStatus Create(FCMRef &ref, FRRef frRefArg, uval pageable,
			    uval preFetchPages = 0,
                            uval maxPages = FCM_MAX_NUMPAGES);

    static SysStatus CreateDefault(FCMRef &ref, FRRef frRefArg, uval pageable,
                                   uval preFetchPages = 0,
                                   uval maxPages = FCM_MAX_NUMPAGES);

    virtual SysStatus fsync(uval force);

    virtual SysStatus doDestroy();

    DEFINE_REFS(FCMFile);
};


#endif /* #ifndef __FCMDEFAULTFILE_H_ */
