#ifndef __PROCESS_DATA_TRANSFER_OBJECT_H_
#define __PROCESS_DATA_TRANSFER_OBJECT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessDataTransferObject.H,v 1.1 2004/08/16 22:00:29 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Data transfer object for hot-swappable Process objects
 * **************************************************************************/

#include <cobj/DTType.H>

template<class ALLOC>
class ProcessDataTransferObject : public DataTransferObject {
    RegionList<ALLOC>       *rlst;
    LazyState               *lazyState;
    class ProcessVPList     *vpList;
    XObjectListProc         *matched;
    SMTMgr                  *memTransMgr;
    HATRef                  hatRef;
    PMRef                   pmRef;
    uval                    osData;
    XObjectListObj          *exported;

public:
    DEFINE_LOCALSTRICT_NEW(ProcessDataTransferObject);
    ProcessDataTransferObject(RegionList<ALLOC> *rlst, LazyState *lazyState,
                              class ProcessVPList *vpList,
                              XObjectListProc *matched, SMTMgr *memTransMgr,
                              HATRef hatRef, PMRef pmRef, uval osData,
                              XObjectListObj *exported)
        : rlst(rlst), lazyState(lazyState), vpList(vpList), matched(matched),
          memTransMgr(memTransMgr), hatRef(hatRef), pmRef(pmRef),
          osData(osData), exported(exported)
        { /* empty body */ }

    RegionList<ALLOC> *getRlst() { return rlst; }
    LazyState *getLazyState() { return lazyState; }
    class ProcessVPList *getVpList() { return vpList; }
    XObjectListProc *getMatched() { return matched; }
    SMTMgr *getMemTransMgr() { return memTransMgr; }
    HATRef getHatRef() { return hatRef; }
    PMRef getPmRef() { return pmRef; }
    uval getOsData() { return osData; }
    XObjectListObj *getExported() { return exported; }
};

#endif /* #ifndef __PROCESS_DATA_TRANSFER_OBJECT_H_ */
