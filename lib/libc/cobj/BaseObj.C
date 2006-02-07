/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseObj.C,v 1.43 2004/10/05 21:28:17 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>

SysStatus
BaseObj::giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,
			    AccessRights match, AccessRights nomatch,
			    TypeID type)
{
    return giveAccessSetClientData(oh, toProcID, match, nomatch, type);
}

SysStatus
BaseObj::giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,
			    AccessRights match, AccessRights nomatch)
{
    return giveAccessByServer(oh, toProcID, match, nomatch, 0);
}

SysStatus
BaseObj::giveAccessInternal(ObjectHandle &oh, ProcessID toProcID,
    AccessRights match, AccessRights nomatch, TypeID type, uval clientData)
{
    tassert(0, err_printf("please don't call me\n"));
    return _SERROR(1130, 0, ENOSYS);

}

SysStatus
BaseObj::giveAccessByServer(ObjectHandle &oh, ProcessID toProcID)
{
    return giveAccessByServer(oh, toProcID, 0, 0, 0);
}

SysStatus
BaseObj::giveAccessByServer(ObjectHandle &oh, ProcessID toProcID, TypeID type)
{
    return giveAccessByServer(oh, toProcID, 0, 0, type);
}

SysStatus
BaseObj::giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
			    AccessRights match, AccessRights nomatch,
			    TypeID type)
{
    tassert(0, err_printf("giveAccessByClient called on BaseObj\n"));
    return _SERROR(1723, 0, ENOSYS);
}

SysStatus
BaseObj::giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
			    AccessRights match, AccessRights nomatch)
{
    return giveAccessByClient(oh, toProcID, match, nomatch, 0);
}

SysStatus
BaseObj::giveAccessByClient(ObjectHandle &oh, ProcessID toProcID)
{
    return giveAccessByClient(oh, toProcID, 0, 0);
}

//FIXME:  Not sure what the intent of this method is.  So removing for
//        the moment.  Complain to me if you need it.
#if 0
SysStatus
BaseObj::unregisterRef()
{
    if (_ref) {
	// FIXME: Jonathan, what should we do here now???
	// DREFGOBJ(TheObjTransRef)->free(_ref);
	_ref = NULL;
    }
    return (0);
}
#endif /* #if 0 */

class DeletedObject {
    static SysStatus deleted() {
	tassertWrn(0,"DeletedObject called\n");
	// see SysStatus.H for deleted object class code
	return _SDELETED(1223);}
public:
    virtual SysStatus method1() {return deleted();}
    virtual SysStatus method2() {return deleted();}
    virtual SysStatus method3() {return deleted();}
    virtual SysStatus method4() {return deleted();}
    virtual SysStatus method5() {return deleted();}
    virtual SysStatus method6() {return deleted();}
    virtual SysStatus method7() {return deleted();}
    virtual SysStatus method8() {return deleted();}
    virtual SysStatus method9() {return deleted();}
    virtual SysStatus method10() {return deleted();}

    virtual SysStatus method11() {return deleted();}
    virtual SysStatus method12() {return deleted();}
    virtual SysStatus method13() {return deleted();}
    virtual SysStatus method14() {return deleted();}
    virtual SysStatus method15() {return deleted();}
    virtual SysStatus method16() {return deleted();}
    virtual SysStatus method17() {return deleted();}
    virtual SysStatus method18() {return deleted();}
    virtual SysStatus method19() {return deleted();}

    virtual SysStatus method20() {return deleted();}
    virtual SysStatus method21() {return deleted();}
    virtual SysStatus method22() {return deleted();}
    virtual SysStatus method23() {return deleted();}
    virtual SysStatus method24() {return deleted();}
    virtual SysStatus method25() {return deleted();}
    virtual SysStatus method26() {return deleted();}
    virtual SysStatus method27() {return deleted();}
    virtual SysStatus method28() {return deleted();}
    virtual SysStatus method29() {return deleted();}
    virtual SysStatus method30() {return deleted();}
    virtual SysStatus method31() {return deleted();}
    virtual SysStatus method32() {return deleted();}
    virtual SysStatus method33() {return deleted();}
    virtual SysStatus method34() {return deleted();}
    virtual SysStatus method35() {return deleted();}
    virtual SysStatus method36() {return deleted();}
    virtual SysStatus method37() {return deleted();}
    virtual SysStatus method38() {return deleted();}
    virtual SysStatus method39() {return deleted();}

    virtual SysStatus method40() {return deleted();}
    virtual SysStatus method41() {return deleted();}
    virtual SysStatus method42() {return deleted();}
    virtual SysStatus method43() {return deleted();}
    virtual SysStatus method44() {return deleted();}
    virtual SysStatus method45() {return deleted();}
    virtual SysStatus method46() {return deleted();}
    virtual SysStatus method47() {return deleted();}
    virtual SysStatus method48() {return deleted();}
    virtual SysStatus method49() {return deleted();}

    virtual SysStatus method50() {return deleted();}
    virtual SysStatus method51() {return deleted();}
    virtual SysStatus method52() {return deleted();}
    virtual SysStatus method53() {return deleted();}
    virtual SysStatus method54() {return deleted();}
    virtual SysStatus method55() {return deleted();}
    virtual SysStatus method56() {return deleted();}
    virtual SysStatus method57() {return deleted();}
    virtual SysStatus method58() {return deleted();}
    virtual SysStatus method59() {return deleted();}

    virtual SysStatus method60() {return deleted();}
    virtual SysStatus method61() {return deleted();}
    virtual SysStatus method62() {return deleted();}
    virtual SysStatus method63() {return deleted();}
    virtual SysStatus method64() {return deleted();}
    virtual SysStatus method65() {return deleted();}
    virtual SysStatus method66() {return deleted();}
    virtual SysStatus method67() {return deleted();}
    virtual SysStatus method68() {return deleted();}
    virtual SysStatus method69() {return deleted();}
    virtual SysStatus method70() {return deleted();}
    virtual SysStatus method71() {return deleted();}
    virtual SysStatus method72() {return deleted();}
    virtual SysStatus method73() {return deleted();}
    virtual SysStatus method74() {return deleted();}
    virtual SysStatus method75() {return deleted();}
    virtual SysStatus method76() {return deleted();}
    virtual SysStatus method77() {return deleted();}
    virtual SysStatus method78() {return deleted();}
    virtual SysStatus method79() {return deleted();}

    virtual SysStatus method80() {return deleted();}
    virtual SysStatus method81() {return deleted();}
    virtual SysStatus method82() {return deleted();}
    virtual SysStatus method83() {return deleted();}
    virtual SysStatus method84() {return deleted();}
    virtual SysStatus method85() {return deleted();}
    virtual SysStatus method86() {return deleted();}
    virtual SysStatus method87() {return deleted();}
    virtual SysStatus method88() {return deleted();}
    virtual SysStatus method89() {return deleted();}

    virtual SysStatus method90() {return deleted();}
    virtual SysStatus method91() {return deleted();}
    virtual SysStatus method92() {return deleted();}
    virtual SysStatus method93() {return deleted();}
    virtual SysStatus method94() {return deleted();}
    virtual SysStatus method95() {return deleted();}
    virtual SysStatus method96() {return deleted();}
    virtual SysStatus method97() {return deleted();}
    virtual SysStatus method98() {return deleted();}
    virtual SysStatus method99() {return deleted();}

    virtual SysStatus method100() {return deleted();}
    virtual SysStatus method101() {return deleted();}
    virtual SysStatus method102() {return deleted();}
    virtual SysStatus method103() {return deleted();}
    virtual SysStatus method104() {return deleted();}
    virtual SysStatus method105() {return deleted();}
    virtual SysStatus method106() {return deleted();}
    virtual SysStatus method107() {return deleted();}
    virtual SysStatus method108() {return deleted();}
    virtual SysStatus method109() {return deleted();}

    virtual SysStatus method110() {return deleted();}
    virtual SysStatus method111() {return deleted();}
    virtual SysStatus method112() {return deleted();}
    virtual SysStatus method113() {return deleted();}
    virtual SysStatus method114() {return deleted();}
    virtual SysStatus method115() {return deleted();}
    virtual SysStatus method116() {return deleted();}
    virtual SysStatus method117() {return deleted();}
    virtual SysStatus method118() {return deleted();}
    virtual SysStatus method119() {return deleted();}

    virtual SysStatus method120() {return deleted();}
    virtual SysStatus method121() {return deleted();}
    virtual SysStatus method122() {return deleted();}
    virtual SysStatus method123() {return deleted();}
    virtual SysStatus method124() {return deleted();}
    virtual SysStatus method125() {return deleted();}
    virtual SysStatus method126() {return deleted();}
    virtual SysStatus method127() {return deleted();}
    virtual SysStatus method128() {return deleted();}
    virtual SysStatus method129() {return deleted();}

    virtual SysStatus method130() {return deleted();}
    virtual SysStatus method131() {return deleted();}
    virtual SysStatus method132() {return deleted();}
    virtual SysStatus method133() {return deleted();}
    virtual SysStatus method134() {return deleted();}
    virtual SysStatus method135() {return deleted();}
    virtual SysStatus method136() {return deleted();}
    virtual SysStatus method137() {return deleted();}
    virtual SysStatus method138() {return deleted();}
    virtual SysStatus method139() {return deleted();}

    virtual SysStatus method140() {return deleted();}
    virtual SysStatus method141() {return deleted();}
    virtual SysStatus method142() {return deleted();}
    virtual SysStatus method143() {return deleted();}
    virtual SysStatus method144() {return deleted();}
    virtual SysStatus method145() {return deleted();}
    virtual SysStatus method146() {return deleted();}
    virtual SysStatus method147() {return deleted();}
    virtual SysStatus method148() {return deleted();}
    virtual SysStatus method149() {return deleted();}

    virtual SysStatus method150() {return deleted();}
    virtual SysStatus method151() {return deleted();}
    virtual SysStatus method152() {return deleted();}
    virtual SysStatus method153() {return deleted();}
    virtual SysStatus method154() {return deleted();}
    virtual SysStatus method155() {return deleted();}
    virtual SysStatus method156() {return deleted();}
    virtual SysStatus method157() {return deleted();}
    virtual SysStatus method158() {return deleted();}
    virtual SysStatus method159() {return deleted();}

    virtual SysStatus method160() {return deleted();}
    virtual SysStatus method161() {return deleted();}
    virtual SysStatus method162() {return deleted();}
    virtual SysStatus method163() {return deleted();}
    virtual SysStatus method164() {return deleted();}
    virtual SysStatus method165() {return deleted();}
    virtual SysStatus method166() {return deleted();}
    virtual SysStatus method167() {return deleted();}
    virtual SysStatus method168() {return deleted();}
    virtual SysStatus method169() {return deleted();}

    virtual SysStatus method170() {return deleted();}
    virtual SysStatus method171() {return deleted();}
    virtual SysStatus method172() {return deleted();}
    virtual SysStatus method173() {return deleted();}
    virtual SysStatus method174() {return deleted();}
    virtual SysStatus method175() {return deleted();}
    virtual SysStatus method176() {return deleted();}
    virtual SysStatus method177() {return deleted();}
    virtual SysStatus method178() {return deleted();}
    virtual SysStatus method179() {return deleted();}

    virtual SysStatus method180() {return deleted();}
    virtual SysStatus method181() {return deleted();}
    virtual SysStatus method182() {return deleted();}
    virtual SysStatus method183() {return deleted();}
    virtual SysStatus method184() {return deleted();}
    virtual SysStatus method185() {return deleted();}
    virtual SysStatus method186() {return deleted();}
    virtual SysStatus method187() {return deleted();}
    virtual SysStatus method188() {return deleted();}
    virtual SysStatus method189() {return deleted();}

    virtual SysStatus method190() {return deleted();}
    virtual SysStatus method191() {return deleted();}
    virtual SysStatus method192() {return deleted();}
    virtual SysStatus method193() {return deleted();}
    virtual SysStatus method194() {return deleted();}
    virtual SysStatus method195() {return deleted();}
    virtual SysStatus method196() {return deleted();}
    virtual SysStatus method197() {return deleted();}
    virtual SysStatus method198() {return deleted();}
    virtual SysStatus method199() {return deleted();}

    virtual SysStatus method200() {return deleted();}
    virtual SysStatus method201() {return deleted();}
    virtual SysStatus method202() {return deleted();}
    virtual SysStatus method203() {return deleted();}
    virtual SysStatus method204() {return deleted();}
    virtual SysStatus method205() {return deleted();}
    virtual SysStatus method206() {return deleted();}
    virtual SysStatus method207() {return deleted();}
    virtual SysStatus method208() {return deleted();}
    virtual SysStatus method209() {return deleted();}

    virtual SysStatus method210() {return deleted();}
    virtual SysStatus method211() {return deleted();}
    virtual SysStatus method212() {return deleted();}
    virtual SysStatus method213() {return deleted();}
    virtual SysStatus method214() {return deleted();}
    virtual SysStatus method215() {return deleted();}
    virtual SysStatus method216() {return deleted();}
    virtual SysStatus method217() {return deleted();}
    virtual SysStatus method218() {return deleted();}
    virtual SysStatus method219() {return deleted();}

    virtual SysStatus method220() {return deleted();}
    virtual SysStatus method221() {return deleted();}
    virtual SysStatus method222() {return deleted();}
    virtual SysStatus method223() {return deleted();}
    virtual SysStatus method224() {return deleted();}
    virtual SysStatus method225() {return deleted();}
    virtual SysStatus method226() {return deleted();}
    virtual SysStatus method227() {return deleted();}
    virtual SysStatus method228() {return deleted();}
    virtual SysStatus method229() {return deleted();}

    virtual SysStatus method230() {return deleted();}
    virtual SysStatus method231() {return deleted();}
    virtual SysStatus method232() {return deleted();}
    virtual SysStatus method233() {return deleted();}
    virtual SysStatus method234() {return deleted();}
    virtual SysStatus method235() {return deleted();}
    virtual SysStatus method236() {return deleted();}
    virtual SysStatus method237() {return deleted();}
    virtual SysStatus method238() {return deleted();}
    virtual SysStatus method239() {return deleted();}

    virtual SysStatus method240() {return deleted();}
    virtual SysStatus method241() {return deleted();}
    virtual SysStatus method242() {return deleted();}
    virtual SysStatus method243() {return deleted();}
    virtual SysStatus method244() {return deleted();}
    virtual SysStatus method245() {return deleted();}
    virtual SysStatus method246() {return deleted();}
    virtual SysStatus method247() {return deleted();}
    virtual SysStatus method248() {return deleted();}
    virtual SysStatus method249() {return deleted();}

    virtual SysStatus method250() {return deleted();}
    virtual SysStatus method251() {return deleted();}
    virtual SysStatus method252() {return deleted();}
    virtual SysStatus method253() {return deleted();}
    virtual SysStatus method254() {return deleted();}
    virtual SysStatus method255() {return deleted();}
};

Obj* theDeletedObj;

/*static*/ void
BaseObj::ClassInit(VPNum vp)
{
    static DeletedObject deletedObject;
    if (vp == 0) {
	theDeletedObj = (Obj*)&deletedObject;
    }
}

SysStatus
BaseObj::destroyUnchecked()
{
    SysStatus retvalue;
    retvalue = DREFGOBJ(TheCOSMgrRef)
	    ->destroyCO((CORef)getRef(), (COSMissHandler *)myRoot);
    return (retvalue);
}

#ifndef NDEBUG
ObjRef
BaseObj::getRef()
{
    //FIXME - this is here because of the tassert, which can't be used
    //        in BaseObj.H because we can't include sysincs.H in BaseObj.H
    ObjRef ref = (ObjRef)CObjRep::getRef();
    tassert(ref, err_printf("myRef not set when getRef called\n"));
    return ref;
}
#endif /* #ifndef NDEBUG */

void *
BaseObj::operator new(size_t size)
{
    tassert(0, err_printf("A new of an object without "
			  "an implementation of new\n"));
    return (void *)0;
}
