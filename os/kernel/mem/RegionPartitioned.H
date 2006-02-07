#ifndef __REGION_PARTITIONED_H_
#define __REGION_PARTITIONED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionPartitioned.H,v 1.1 2004/11/01 19:07:56 dilma Exp $
 *****************************************************************************/
/******************************************************************************
/*****************************************************************************
 * Module Description:  Class for object invocation of Region
 *
 *  Implements distributed requests counters via proxy representatives
 * **************************************************************************/

#include "mem/RegionDefault.H"
#include "cobj/CObjRootMultiRep.H"

#define REDIRECTTO(m) (firstRep->m)
class RegionDefaultMultiRep;

class RegionDefaultMultiRep : public RegionDefault
{
    class RegionDefaultMultiRepRoot : public CObjRootMultiRep {
        friend class RegionDefaultMultiRep;
        class Proxy;
        // Structure for head pointer to proxy list where we steal
        // the bottom two bits to track the state of the Request Counters
        struct State : public BitStructure {
            __BIT_FIELD((8*sizeof(Proxy *))-2,
                        proxyListHeadBits, BIT_FIELD_START);
            __BIT_FIELD(2, rcState, proxyListHeadBits);
            enum RCStates {NORMAL=0, STOP=1, SHUTDOWN=2}; 
        Proxy *proxyListHead()     
            {
                // to turn bits back into a pointer must shift them up 2
                return (Proxy *)(proxyListHeadBits()<<2);
            }
        void  proxyListHead(Proxy *head) 
            { 
                // chop off bottom two bits from pointer
                return proxyListHeadBits(uval64(head)>>2);
            }
        } currentState;

        RegionDefaultMultiRep *firstRep;

        void init() 
            {
                VPNum vp=Scheduler::GetVP();
                currentState.data = uval64(0);
                // WARNING ASSUMING THAT init is called before any misses
                // can occur, hence calling locked_addRep without lock!!!!
                locked_addRep(vp,firstRep);
            }

        RegionDefaultMultiRepRoot(RegionDefaultMultiRep *rep) : 
            CObjRootMultiRep(), firstRep(rep) {
            init();
        }

        RegionDefaultMultiRepRoot(RegionDefaultMultiRep *rep, RegionRef ref) : 
            CObjRootMultiRep((RepRef)ref), firstRep(rep) {
            init();
        }
        
        class Proxy : public Region {
            friend class RegionDefaultMultiRepRoot;
            RegionDefaultMultiRep *firstRep;
            Proxy *nxt;
            RequestCountWithStop requests;
            Proxy(RegionDefaultMultiRep *fRep, Proxy *n) :
                firstRep(fRep), nxt(n) {}
            
            Proxy * getNext()       { return nxt; }
            sval enterRequest()     { return requests.enter(); }
            void leaveRequest()     { return requests.leave(); }
            sval stopRequests()     { return requests.stop(); }
            void restartRequests()  { return requests.restart(); } 
            sval shutdownRequests() { return requests.shutdown(); }
            
            DEFINE_LOCALSTRICT_NEW(Proxy);
        public:
            virtual SysStatus getVaddr(uval &vaddr) 
                { return REDIRECTTO(getVaddr(vaddr)); }
            
            virtual void      setVaddr(uval v) 
                { REDIRECTTO(setVaddr(v)); }
            
            virtual void      setOffset(uval v) 
                { REDIRECTTO(setOffset(v)); }
            
            virtual SysStatus getSize(uval &size) 
                { return REDIRECTTO(getSize(size)); }
            
            virtual SysStatus isSharedOffset(uval offset) 
                { return REDIRECTTO(isSharedOffset(offset)); }
            
            virtual SysStatus isSharedVaddr(uval vaddr) 
                { return REDIRECTTO(isSharedVaddr(vaddr)); }

            /*
             * handle a page table miss (not a tlb miss)
             * ==0 in-core, > 0 asynch, < 0 error
             */
            virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
                                              uval vaddr, 
                                              PageFaultNotification *pn,
                                              VPNum vp) 
                {
                    SysStatusUval rc;
                    if (enterRequest() < 0) {
                        //FIXME return correct error for can't map page
                        //N.B. when enter fails, DO NOT leave()
                        return _SERROR(1214, 0, EFAULT);
                    }
                    rc=REDIRECTTO(requestCounted_handleFault(pfinfo,vaddr,
                                                             pn,vp));
                    leaveRequest();
                    return (rc);
                }

            virtual SysStatus unmapPage(uval offset) 
                { return REDIRECTTO(unmapPage(offset)); }
            
            virtual SysStatus unmapRange(uval start, uval size)
                { return REDIRECTTO(unmapRange(start,size)); }

            virtual SysStatus vaddrToFCM(
                VPNum vpNum, uval vaddr, uval writeAccess,
                FCMRef& fcmRef, uval& offset)
                { return REDIRECTTO(vaddrToFCM(vpNum,
                                               vaddr,
                                               writeAccess,fcmRef,offset));}
            virtual SysStatus destroy()
                { return REDIRECTTO(destroy()); }
            
            virtual SysStatus forkCloneRegion(
                ProcessRef pref, RegionType::Type regionType) 
                { return REDIRECTTO(forkCloneRegion(pref,regionType)); }

            virtual SysStatus forkCopy(FRRef& fr)
                { return REDIRECTTO(forkCopy(fr)); }
            
            virtual SysStatus truncate(uval start, uval length)
                { return REDIRECTTO(truncate(start,length)); }

            virtual SysStatusProcessID getPID()
                { return REDIRECTTO(getPID()); }
        };
        DEFINE_GLOBALPADDED_NEW(RegionDefaultMultiRepRoot);
    public:
        virtual CObjRep * createRep(VPNum vp) 
            {
                Proxy *proxy=0;
                State oldState,newState;
                // Atomically add a proxy
                // Begining to wonder if we should ever have had built in
                // repLists (not sure it was ever necessary) only adds headaches
                do {
                    oldState = currentState;
                    newState = oldState;
                    if (proxy) delete proxy;
                    proxy=new Proxy(firstRep,newState.proxyListHead());
                    if (newState.rcState()==State::STOP) {
                        proxy->requests.stop();
                    } else if (newState.rcState()==State::SHUTDOWN) {
                        proxy->requests.shutdown();
                    } 
                    newState.proxyListHead(proxy);
                } while (!CompareAndStore64Synced(&currentState.data,
                                                  oldState.data,
                                                  newState.data));
                return (CObjRep *)proxy;
            }


        sval stopOtherRequests() 
            {
                State oldState, newState;
                sval rtn=0;
                // update request counter state.
                // doing so now ensures that any new 
                // proxys added will be in the right state
                do {
                    oldState = currentState;
                    newState = oldState;
                    newState.rcState(State::STOP);
                } while (!CompareAndStore64Synced(&currentState.data,
                                                  oldState.data,
                                                  newState.data));
                // now update the request counters of the proxys
                // that exist to match
                for (Proxy *p=newState.proxyListHead();
                     p!=0;
                     p=p->getNext()) {
                    rtn+=p->stopRequests();
                    tassertMsg(rtn==0, "oops proxy already stopped\n");
                }
                return rtn;
            }

        void restartOtherRequests()
            {
                State oldState, newState;

                // update request counter state.
                // doing so now ensures that any new 
                // proxys added will be in the right state
                do {
                    oldState = currentState;
                    newState = oldState;
                    newState.rcState(State::NORMAL);
                } while (!CompareAndStore64Synced(&currentState.data,
                                                  oldState.data,
                                                  newState.data));
                // now update the request counters of the proxys
                // that exist to match
                for (Proxy *p=newState.proxyListHead();
                     p!=0;
                     p=p->getNext()) {
                    p->restartRequests();
                }
            }

        sval shutdownOtherRequests()
            {
                State oldState, newState;
                sval rtn=0;
                // update request counter state.
                // doing so now ensures that any new 
                // proxys added will be in the right state
                do {
                    oldState = currentState;
                    newState = oldState;
                    newState.rcState(State::SHUTDOWN);
                } while (!CompareAndStore64Synced(&currentState.data,
                                                  oldState.data,
                                                  newState.data));
                // now update the request counters of the proxys
                // that exist to match
                for (Proxy *p=newState.proxyListHead();
                     p!=0;
                     p=p->getNext()) {
                    rtn+=p->shutdownRequests();
                    tassertMsg(rtn==0, "oops proxy already stopped\n");
                }
                return rtn;
            }

    };
    
    RegionDefaultMultiRepRoot *root() 
        {
            return (RegionDefaultMultiRepRoot *)myRoot;
        }
protected:
    virtual sval enterRequest()     { return requests.enter(); }
    virtual void leaveRequest()     { return requests.leave(); }

    virtual sval stopRequests()     
        { 
            sval rc=requests.stop();
            if (rc < 0) return rc;
            rc=COGLOBAL(stopOtherRequests()); 
            tassertMsg(rc==0, "if we go to the proxy's then this can not fail");
            return 0;
        }
     virtual void restartRequests()  
        {
            // WARNING: Order of the next two lines matters
            // must restart firstRep after others
            // to avoid races between restarts in 
            // in progress with new attempts to 
            // shutdown or stop the counters
            // Therefore ensure restart is complete
            // before we let any new stop or shutdown
            // get through.
            COGLOBAL(restartOtherRequests()); 
            requests.restart();
        }
    virtual sval shutdownRequests() 
        { 
           sval rc=requests.shutdown();
           if (rc < 0) return rc;
           rc=COGLOBAL(shutdownOtherRequests()); 
           tassertMsg(rc==0, "if we go to the proxy's then this can not fail");
           return 0;
        }
public:
    static RegionRef Create(RegionDefaultMultiRep *rep, RegionRef use_this_ref)
        {
            RegionDefaultMultiRepRoot *root;
            if (use_this_ref==NULL) {
                root=new RegionDefaultMultiRepRoot(rep);
            } else {
                root=new RegionDefaultMultiRepRoot(rep,use_this_ref);
            }
            return (RegionRef)root->getRef();
        }

        DEFINE_GLOBALPADDED_NEW(RegionDefaultMultiRep);

};


#endif /* #ifndef __REGION_PARTITIONED_H_ */

