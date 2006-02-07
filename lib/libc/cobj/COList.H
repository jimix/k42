#ifndef __CO_LIST_H__
#define __CO_LIST_H__
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COList.H,v 1.2 2005/03/02 05:27:55 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/****************************************************************************
 * Module Description: 
 * **************************************************************************/
#include <stub/StubCOListServer.H>
#include <cobj/Factory.H>

DEFINE_REFTYPE(COList);

class COList : public CObjRep {
    BLock lock;
    ProcessID srvPID;
    StubCOListServer coLstSrv;
    CODesc *coDescArray;
    COList() : coLstSrv(StubObj::UNINITIALIZED), coDescArray(0) {}
    SysStatus init(ProcessID pid);
    DEFINE_GLOBALPADDED_NEW(COList);
public:
    static SysStatus Create(COListRef &ref, ProcessID pid);

    virtual SysStatus getAndLockCODescArray(CODesc **array, uval &num) 
        {
            // FIXME: Super kludgy syncronization fix with a better 
            //        abstraction and implementation
            lock.acquire();
            SysStatus rc = coLstSrv.updateCODescs(num);
            tassertMsg(_SUCCESS(rc), "Yikes rc=%ld\n", rc);
            *array = coDescArray; 
            return rc; 
        }    

    virtual SysStatus unlockCODescArray() 
        {
            lock.release();
            return 0;
        }
    virtual SysStatus hotSwapInstance(FactoryRef fac, CObjRoot *targetRoot)
        {
            return coLstSrv.hotSwapInstance((uval)fac, (uval)targetRoot);
        }

    virtual SysStatus takeOverFromFac(FactoryRef newFac, FactoryRef oldFac) {
        return coLstSrv.takeOverFromFac((uval)newFac, (uval)oldFac);
    }

    virtual SysStatus printInstances(FactoryRef fac) {
        return coLstSrv.printInstances((uval)fac);
    }

    virtual SysStatus readMem(uval addr, uval & len, char *buf) {
        return coLstSrv.readMem(addr, len, buf);
    }
};
#endif /* #ifndef __CO_LIST_H__ */
