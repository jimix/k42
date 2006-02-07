/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Bonnie.C,v 1.54 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/ppccore.H>
#include "Bonnie.H"
#include "proc/Process.H"
#include <cobj/CObjRootSingleRep.H>

/* The Static Functions and Constructurs */

void
Bonnie::init()
{
    MetaBonnie::init();
    MetaBonnieGrandChild::init();
    MetaBadgeTester::init();
}

Bonnie::Bonnie()
{
    /* we assume that every new object will be entered into the OT */
    CObjRootSingleRep::Create(this);
    cprintf("Creating Bonnie<%p>\n",this);
}


SysStatus
Bonnie::construct(__out ObjectHandle & oh, int a, int b,
		  __CALLER_PID processID)
{
    SysStatus retvalue;
    cprintf("Bonnie::construct(ObjectHandle OUT oh, %d, %d)\n",a,b);
    Bonnie *obj = new Bonnie();
    tassert( (processID == _SGETPID(DREFGOBJK(TheProcessRef)->getPID())),
	     err_printf("should be internal\n") );
    retvalue = obj->giveAccessByServer(oh, processID,MetaBonnie::none,
				   MetaBonnie::none);
    return(retvalue);
}

SysStatus
Bonnie::construct(__out ObjectHandle & oh, int a, int b, int c,
		  __CALLER_PID processID)
{
    SysStatus retvalue;
    cprintf("Bonnie::construct(OUT oh, %d, %d, %d)\n",a,b,c);
    Bonnie *obj = new Bonnie();
    tassert( (processID == _SGETPID(DREFGOBJK(TheProcessRef)->getPID())),
	     err_printf("should be internal\n") );
    retvalue = obj->giveAccessByServer(oh, processID,MetaBonnie::none,
				   MetaBonnie::none);
    return(retvalue);
}

SysStatus
Bonnie::construct(__out ObjectHandle & oh, __CALLER_PID processID)
{
    SysStatus retvalue;
    cprintf("Bonnie::construct(ObjectHandle OUT oh)\n");
    Bonnie *obj = new Bonnie();
    tassert( (processID == _SGETPID(DREFGOBJK(TheProcessRef)->getPID())),
	     err_printf("should be internal\n") );
    retvalue = obj->giveAccessByServer(oh, processID,MetaBonnie::none,
				   MetaBonnie::none);
    return(retvalue);
}

SysStatus
Bonnie::aStaticFunc(const long arg)
{
    cprintf("Bonnie::aStaticFunc(const long arg=%ld)\n",arg);
    return(0);
}

/* the virtual or Object Functions exported by this class */

SysStatus
Bonnie::null()
{
    return(0);
}

SysStatus
Bonnie::simple(int a1, int a2, int a3, int a4, int a5)
{
    cprintf("Bonnie<%p>::simple(%d,%d,%d,%d,%d)\n",this,a1,a2,a3,
		    a4,a5);
    return(0);
}

SysStatus
Bonnie::simple(__in int)
{
    return(0);
}

SysStatus
Bonnie::reftest(__inout long & arg)
{
    cprintf("Bonnie<%p>::reftest(%ld)\n",this,arg);
    arg--;
    return(0);
}

SysStatus
Bonnie::reftest(__inout long & , __out long & , long)
{
    return(0);
}

SysStatus
Bonnie::steve(long , __inout ohh * , __inout ohh &)
{
    return(0);
}

SysStatus
Bonnie::func1(char , char , unsigned short)
{
    return(0);
}

SysStatus
Bonnie::string0 (__inbuf(*)          char*)
{
    return(0);
}

SysStatus
Bonnie::string1 (__inbuf(*)          char* arg1,
		  __inbuf(*)          char* arg2)
{
    cprintf("Bonnie<%p>::string(%s,%s)[%p,%p\n",this,arg1,arg2,
		    arg1,arg2);
    uval len = strlen(arg2);
    strncpy(arg2,"RETURNSTRING",len);
    return(0);
}

SysStatus
Bonnie::string2 (__inbuf(*)          char* arg1,
		  __inoutbuf(*:*:20)  char* arg2)
{
    cprintf("Bonnie<%p>::string(%s,%s)[%p,%p\n",this,arg1,arg2,
		    arg1,arg2);
    uval len = strlen(arg2);
    strncpy(arg2,"RETURNSTRING",len);
    return(0);
}

SysStatus
Bonnie::string3 (__inbuf(*)          char* /*arg1*/,
		  __inoutbuf(*:*:20)  char* /*arg2*/,
		  __outbuf(*:30)      char* /*arg3*/)
{
    return(0);
}

SysStatus
Bonnie::string4 (__inoutbuf(*:*:20)  char* arg1,
		  __outbuf(*:25)      char* arg2,
		  __inoutbuf(*:*:30) char* arg3,
		  __outbuf(*:35)     char* arg4)
{
    cprintf("Bonnie::string4>> <%s> <%s>\n",arg1,arg3);
    strcat(arg1,"XXXXXX");
    strcat(arg3,"YYYYYY");
    strcpy(arg2,"OUTBUG_ARG2");
    strcpy(arg4,"OUTBUG_ARG4");
    cprintf("Bonnie::string4<< <%s> <%s> <%s> <%s>\n",arg1,arg2,
		    arg3,arg4);
    return(0);
}

SysStatus
Bonnie::string5 (__outbuf(*:25)       char* arg1,
		  __outbuf(*:30)       char* arg2,
		  __outbuf(*:35)       char* arg3,
		  __outbuf(*:40)       char* arg4)
{
    strcpy(arg1,"OUTBUG_ARG1");
    strcpy(arg2,"OUTBUG_ARG2");
    strcpy(arg3,"OUTBUG_ARG3");
    strcpy(arg4,"OUTBUG_ARG4");
    return(0);
}

SysStatus
Bonnie::cohhr (ohh & coord)
{
    cprintf("Bonnie<%p>::cohh([%ld,%ld])\n",this,coord.x,coord.y);
    state = state+1;
    coord.x *= 2; coord.y *= 2;
    return(0);
}

SysStatus
Bonnie::cohhp (ohh * coord)
{
    cprintf("Bonnie<%p>::cohh([%ld,%ld])\n",this,coord->x,coord->y);
    state = state+1;
    coord->x *= 2; coord->y *= 2;
    return(0);
}

SysStatus
Bonnie::m2ohh (ohh & coord, uval & rstate)
{
    cprintf("Bonnie<%p>::m2ohh([%ld,%ld])\n",this,coord.x,coord.y);
    state = state+1;
    coord.x *= 2; coord.y *= 2;
    rstate = state;
    return(0);
}

SysStatus
Bonnie::arraytest (__inbuf(3)  uval *arg1,
		    __outbuf(5) uval *arg2)
{
    cprintf("Bonnie<%p>::arraytest([%ld,%ld,%ld])\n",
		    this,arg1[0],arg1[1],arg1[2]);
    for (int i=0; i < 5; i++) arg2[i] = 3000+i;
    return(0);
}

SysStatus
Bonnie::inout (__inbuf(*)          char* lsArgs,
	       __outbuf(*:1024)    char* outputBuf)
{
    cprintf("Bonnie<%p>::inout(%p,%s)\n",
		    this,lsArgs,lsArgs);
    strcpy(outputBuf,"this_is_the_inout_result");
    return(0);
}

SysStatus
Bonnie::varray(__inbuf(len)        uval *vax,
	       __in                uval  len,
	       __inbuf(*)          char* sayhello)
{
    cprintf("Bonnie<%p>::varray(%p,%ld) >%s<\n",
		    this,vax,len,sayhello);
    for (uval i=0; i < len; i++)
	cprintf("\tvarray_%p[%ld]=%ld\n",vax,i,vax[i]);
    return(0);
}

SysStatus
Bonnie::varray(__outbuf(len3:1024)  uval */*va1*/,
	       __inbuf(len2)        uval */*va2*/,
	       __outbuf(*:len2)     char *va3,
	       __inbuf(*)           char */*va4*/,
	       __inoutbuf(3:4:100)  uval */*va5*/,
	       __inoutbuf(len1:len2:32)   uval */*va6*/,
	       __in                 uval  /*len1*/,
	       __inout              uval *len2,
	       __out                uval &len3,
	       __inbuf(20)          uval */*stuff*/)
{
    len3=0;
    len2=0;
    strcpy(va3,"");
    return(0);
}

SysStatus
Bonnie::overflow(__inoutbuf(*:*:len)     char *va,
		 __inout                  uval *len)
{
    cprintf("Bonnie<%p>::overflow(%s,%ld)\n",this,va,*len);
    return(0);
}

SysStatus
Bonnie::ppc_overflow(__inoutbuf(ilen:olen:mlen) uval *va,
		     __in                       uval /*ilen*/,
		     __out                      uval *olen,
		     __in                       uval mlen)
{
    cprintf("Bonnie<%p>::ppc_overflow(%s,%ld)\n",this,(char *)va,mlen);
    *olen = 10000;
    return(0);
}

SysStatus
Bonnie::garray(__inoutbuf(len:len:lenmax)  uval *va,
	       __inout               uval &len,
	       __in                  uval  lenmax)
{
    cprintf("Bonnie<%p>::garray(%p,%ld,%ld)\n",this,va,len,lenmax);
    len = 10; /* but we only fill 5 for testing */
    for (int i=0; i < 5; i++) va[i] = 3000+i;
    return(0);
}

SysStatus
Bonnie::getName(__inoutbuf(*:*:len) char *va,
			  __in                uval  /*len*/)
{
    strcpy(va,"Bonnie");
    return(0);
}

SysStatus
Bonnie::testBadge(uval val, __CALLER_PID processID)
{
    cprintf("Bonnie<%p>::testBadge(val=%ld,processID=0x%lx)\n",
		    this,val,processID);
    return(0);
}

SysStatus
BonnieChild::getName(__inoutbuf(*:*:len) char *va,
		     __in                uval  /*len*/)
{
    strcpy(va,"BonnieChild");
    return(0);
}

SysStatus
BonnieGrandChild::construct(__out ObjectHandle & oh)
{
    cprintf("BonnieGrandChild::construct(ObjectHandle OUT oh)\n");
    BonnieGrandChild *obj = new BonnieGrandChild();
    /* first we put ourselfs into the OT */
    ObjRef obri = (ObjRef) CObjRootSingleRep::Create(obj);
    XHandle obrx = MetaBonnieGrandChild::createXHandle(
	obri, GOBJ(TheProcessRef),0,0);

    oh.initWithMyPID(obrx);
    cprintf("BonnieGC<%p> => XBonnieGC<hd=0x%lx ir=%p>\n",
		    obj,obrx,obri);
    return(0);
}



SysStatus
BadgeTester::construct(__out ObjectHandle & oh,
		       __in  AccessRights rights,
		       __CALLER_PID processID)
{
    // only one object will be created for this class
    static BadgeTester *obj = NULL;

    if (obj == NULL) {
	obj = new BadgeTester();
	CObjRootSingleRep::Create(obj);
    }

    cprintf("BadgeTester<%p>::construct(processID=0x%lx,rights=%ld)\n",
	    obj,processID,rights);

    // enter ourselves into the OT and remember where


    tassert( (processID == _SGETPID(DREFGOBJK(TheProcessRef)->getPID())),
	     err_printf("should be internal\n") );

    return obj->giveAccessByServer(oh,processID,rights,MetaObj::none);
}

SysStatus
BadgeTester::testRead()
{
    cprintf("BadgeTester<%p>::testRead()\n",this);
    return(0);
}

SysStatus
BadgeTester::testWrite()
{
    cprintf("BadgeTester<%p>::testWrite()\n",this);
    return(0);
}

SysStatus
BadgeTester::testReadWrite()
{
    cprintf("BadgeTester<%p>::testReadWrite()\n",this);
    return(0);
}

SysStatus
BadgeTester::testAny()
{
    cprintf("BadgeTester<%p>::testAny()\n",this);
    return(0);
};
